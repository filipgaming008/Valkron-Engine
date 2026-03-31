#include "Renderer/UIRenderer.hpp"

#include "Core/FileSystem.hpp"
#include "Core/Log.hpp"
#include "Renderer/Shader.hpp"

#include "glad/gl.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace Valkron {

    struct GlyphInfo {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
        float advance = 0.0f;
        bool valid = false;
    };

    struct FontCacheHeader {
        std::uint32_t magic = 0;
        std::uint32_t version = 0;
        std::uint32_t atlasWidth = 0;
        std::uint32_t atlasHeight = 0;
        float atlasPixelHeight = 0.0f;
        float lineHeightPx = 0.0f;
        float ascentPx = 0.0f;
        std::uint32_t glyphCount = 0;
        std::uint32_t atlasByteCount = 0;
    };

    struct CachedGlyphInfo {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
        float advance = 0.0f;
        std::uint8_t valid = 0;
        std::uint8_t reserved0 = 0;
        std::uint8_t reserved1 = 0;
        std::uint8_t reserved2 = 0;
    };

    constexpr std::uint32_t kFontCacheMagic = 0x54414646; // 'FFAT'
    constexpr std::uint32_t kFontCacheVersion = 1;
    constexpr int kAtlasWidth = 1024;
    constexpr int kAtlasHeight = 1024;
    constexpr int kGlyphPadding = 2;
    constexpr int kAtlasPixelHeight = 64;
    constexpr int kAsciiFirst = 32;
    constexpr int kAsciiLast = 126;

    struct UIRendererData {
        std::unique_ptr<Shader> shader;
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        unsigned int fontTexture = 0;
        int atlasWidth = 0;
        int atlasHeight = 0;
        float atlasPixelHeight = static_cast<float>(kAtlasPixelHeight);
        float lineHeightPx = static_cast<float>(kAtlasPixelHeight);
        float ascentPx = static_cast<float>(kAtlasPixelHeight) * 0.75f;
        std::array<GlyphInfo, 128> glyphs{};
        bool fontReady = false;
        std::vector<UIVertex> pendingVertices;
        std::vector<std::uint32_t> pendingIndices;
        bool initialized = false;
    };

    static UIRendererData s_data;

    static std::filesystem::path getCachePath() {
        return FileSystem::resolveAssetPath("fonts/cache/ui_font_cache.bin");
    }

    static std::filesystem::path resolveFontPath() {
        const std::filesystem::path assetFont = FileSystem::resolveAssetPath("fonts/DejaVuSans.ttf");
        std::error_code ec;
        if (std::filesystem::exists(assetFont, ec)) {
            return assetFont;
        }

        #if defined(_WIN32)
                const std::array<std::filesystem::path, 5> candidatePaths = {
                    "C:/Windows/Fonts/segoeui.ttf",
                    "C:/Windows/Fonts/arial.ttf",
                    "C:/Windows/Fonts/calibri.ttf",
                    "C:/Windows/Fonts/tahoma.ttf",
                    "C:/Windows/Fonts/verdana.ttf"
                };
        #else
                const std::array<std::filesystem::path, 6> candidatePaths = {
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                    "/usr/share/fonts/TTF/DejaVuSans.ttf",
                    "/usr/local/share/fonts/DejaVuSans.ttf",
                    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
                    "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
                };
        #endif

        for (const auto& path : candidatePaths) {
            if (std::filesystem::exists(path, ec)) {
                return path;
            }
        }

        return {};
    }

    static bool uploadAtlasTexture(const std::vector<unsigned char>& atlasBytes, int width, int height) {
        if (atlasBytes.empty() || width <= 0 || height <= 0) {
            return false;
        }

        if (s_data.fontTexture == 0) {
            glGenTextures(1, &s_data.fontTexture);
        }
        VALKRON_CORE_ASSERT(s_data.fontTexture != 0, "Failed to create font texture");

        glBindTexture(GL_TEXTURE_2D, s_data.fontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R8,
            width,
            height,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            atlasBytes.data()
        );
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    static bool loadAtlasCache() {
        const std::filesystem::path cachePath = getCachePath();
        std::ifstream cacheFile(cachePath, std::ios::binary);
        if (!cacheFile.is_open()) {
            return false;
        }

        FontCacheHeader header{};
        cacheFile.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!cacheFile || header.magic != kFontCacheMagic || header.version != kFontCacheVersion) {
            return false;
        }

        if (header.glyphCount != s_data.glyphs.size()) {
            return false;
        }

        std::vector<CachedGlyphInfo> cachedGlyphs(header.glyphCount);
        cacheFile.read(reinterpret_cast<char*>(cachedGlyphs.data()), static_cast<std::streamsize>(cachedGlyphs.size() * sizeof(CachedGlyphInfo)));
        if (!cacheFile) {
            return false;
        }

        std::vector<unsigned char> atlasBytes(header.atlasByteCount);
        cacheFile.read(reinterpret_cast<char*>(atlasBytes.data()), static_cast<std::streamsize>(atlasBytes.size()));
        if (!cacheFile) {
            return false;
        }

        s_data.atlasWidth = static_cast<int>(header.atlasWidth);
        s_data.atlasHeight = static_cast<int>(header.atlasHeight);
        s_data.atlasPixelHeight = header.atlasPixelHeight;
        s_data.lineHeightPx = header.lineHeightPx;
        s_data.ascentPx = header.ascentPx;

        for (std::size_t i = 0; i < s_data.glyphs.size(); ++i) {
            const CachedGlyphInfo& in = cachedGlyphs[i];
            GlyphInfo& out = s_data.glyphs[i];
            out.u0 = in.u0;
            out.v0 = in.v0;
            out.u1 = in.u1;
            out.v1 = in.v1;
            out.width = in.width;
            out.height = in.height;
            out.xOffset = in.xOffset;
            out.yOffset = in.yOffset;
            out.advance = in.advance;
            out.valid = (in.valid != 0);
        }

        if (!uploadAtlasTexture(atlasBytes, s_data.atlasWidth, s_data.atlasHeight)) {
            return false;
        }

        s_data.fontReady = true;
        LOG_INFO("Loaded cached UI font atlas: " + cachePath.string());
        return true;
    }

    static bool saveAtlasCache(const std::vector<unsigned char>& atlasBytes) {
        const std::filesystem::path cachePath = getCachePath();
        std::error_code ec;
        std::filesystem::create_directories(cachePath.parent_path(), ec);

        std::ofstream cacheFile(cachePath, std::ios::binary | std::ios::trunc);
        if (!cacheFile.is_open()) {
            return false;
        }

        FontCacheHeader header{};
        header.magic = kFontCacheMagic;
        header.version = kFontCacheVersion;
        header.atlasWidth = static_cast<std::uint32_t>(s_data.atlasWidth);
        header.atlasHeight = static_cast<std::uint32_t>(s_data.atlasHeight);
        header.atlasPixelHeight = s_data.atlasPixelHeight;
        header.lineHeightPx = s_data.lineHeightPx;
        header.ascentPx = s_data.ascentPx;
        header.glyphCount = static_cast<std::uint32_t>(s_data.glyphs.size());
        header.atlasByteCount = static_cast<std::uint32_t>(atlasBytes.size());

        cacheFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

        std::vector<CachedGlyphInfo> cachedGlyphs(s_data.glyphs.size());
        for (std::size_t i = 0; i < s_data.glyphs.size(); ++i) {
            const GlyphInfo& in = s_data.glyphs[i];
            CachedGlyphInfo& out = cachedGlyphs[i];
            out.u0 = in.u0;
            out.v0 = in.v0;
            out.u1 = in.u1;
            out.v1 = in.v1;
            out.width = in.width;
            out.height = in.height;
            out.xOffset = in.xOffset;
            out.yOffset = in.yOffset;
            out.advance = in.advance;
            out.valid = in.valid ? 1 : 0;
        }

        cacheFile.write(reinterpret_cast<const char*>(cachedGlyphs.data()), static_cast<std::streamsize>(cachedGlyphs.size() * sizeof(CachedGlyphInfo)));
        cacheFile.write(reinterpret_cast<const char*>(atlasBytes.data()), static_cast<std::streamsize>(atlasBytes.size()));

        return static_cast<bool>(cacheFile);
    }

    static bool generateAtlasFromFreeType() {
        const std::filesystem::path fontPath = resolveFontPath();
        if (fontPath.empty()) {
            LOG_WARN("No font file found for UI text atlas generation; text rendering disabled.");
            return false;
        }

        FT_Library ft = nullptr;
        if (FT_Init_FreeType(&ft) != 0) {
            LOG_WARN("Failed to initialize FreeType; text rendering disabled.");
            return false;
        }

        FT_Face face = nullptr;
        const std::string fontPathString = fontPath.string();
        if (FT_New_Face(ft, fontPathString.c_str(), 0, &face) != 0) {
            FT_Done_FreeType(ft);
            LOG_WARN("Failed to load font via FreeType: " + fontPathString);
            return false;
        }

        if (FT_Set_Pixel_Sizes(face, 0, kAtlasPixelHeight) != 0) {
            FT_Done_Face(face);
            FT_Done_FreeType(ft);
            LOG_WARN("Failed to set font pixel size via FreeType.");
            return false;
        }

        s_data.atlasPixelHeight = static_cast<float>(kAtlasPixelHeight);
        s_data.lineHeightPx = static_cast<float>(face->size->metrics.height >> 6);
        s_data.ascentPx = static_cast<float>(face->size->metrics.ascender >> 6);
        if (s_data.lineHeightPx <= 0.0f) {
            s_data.lineHeightPx = s_data.atlasPixelHeight;
        }
        if (s_data.ascentPx <= 0.0f) {
            s_data.ascentPx = s_data.atlasPixelHeight * 0.75f;
        }

        s_data.atlasWidth = kAtlasWidth;
        s_data.atlasHeight = kAtlasHeight;
        s_data.glyphs = {};

        std::vector<unsigned char> atlasBytes(static_cast<std::size_t>(s_data.atlasWidth * s_data.atlasHeight), 0);

        int penX = kGlyphPadding;
        int penY = kGlyphPadding;
        int rowHeight = 0;
        int generatedGlyphCount = 0;

        for (int codepoint = kAsciiFirst; codepoint <= kAsciiLast; ++codepoint) {
            if (FT_Load_Char(face, static_cast<FT_ULong>(codepoint), FT_LOAD_RENDER) != 0) {
                continue;
            }

            const FT_GlyphSlot glyphSlot = face->glyph;
            const FT_Bitmap& bitmap = glyphSlot->bitmap;
            const int glyphWidth = static_cast<int>(bitmap.width);
            const int glyphHeight = static_cast<int>(bitmap.rows);

            if (glyphWidth == 0 || glyphHeight == 0) {
                GlyphInfo& glyph = s_data.glyphs[static_cast<std::size_t>(codepoint)];
                glyph.advance = static_cast<float>(glyphSlot->advance.x >> 6);
                glyph.valid = false;
                continue;
            }

            if (penX + glyphWidth + kGlyphPadding > s_data.atlasWidth) {
                penX = kGlyphPadding;
                penY += rowHeight + kGlyphPadding;
                rowHeight = 0;
            }

            if (penY + glyphHeight + kGlyphPadding > s_data.atlasHeight) {
                LOG_WARN("FreeType UI atlas capacity exceeded; some glyphs are missing.");
                break;
            }

            const int srcPitch = std::abs(static_cast<int>(bitmap.pitch));
            for (int row = 0; row < glyphHeight; ++row) {
                const int srcRow = (bitmap.pitch < 0) ? (glyphHeight - 1 - row) : row;
                const unsigned char* src = bitmap.buffer + srcRow * srcPitch;
                unsigned char* dst = atlasBytes.data() + static_cast<std::size_t>((penY + row) * s_data.atlasWidth + penX);
                std::copy(src, src + glyphWidth, dst);
            }

            GlyphInfo& glyph = s_data.glyphs[static_cast<std::size_t>(codepoint)];
            glyph.u0 = static_cast<float>(penX) / static_cast<float>(s_data.atlasWidth);
            glyph.v0 = static_cast<float>(penY) / static_cast<float>(s_data.atlasHeight);
            glyph.u1 = static_cast<float>(penX + glyphWidth) / static_cast<float>(s_data.atlasWidth);
            glyph.v1 = static_cast<float>(penY + glyphHeight) / static_cast<float>(s_data.atlasHeight);
            glyph.width = static_cast<float>(glyphWidth);
            glyph.height = static_cast<float>(glyphHeight);
            glyph.xOffset = static_cast<float>(glyphSlot->bitmap_left);
            glyph.yOffset = static_cast<float>(-glyphSlot->bitmap_top);
            glyph.advance = static_cast<float>(glyphSlot->advance.x >> 6);
            glyph.valid = true;

            penX += glyphWidth + kGlyphPadding;
            rowHeight = std::max(rowHeight, glyphHeight);
            ++generatedGlyphCount;
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        if (!uploadAtlasTexture(atlasBytes, s_data.atlasWidth, s_data.atlasHeight)) {
            return false;
        }

        s_data.fontReady = true;
        saveAtlasCache(atlasBytes);
        if (generatedGlyphCount == 0) {
            LOG_WARN("Generated FreeType atlas but no glyphs were produced.");
        }
        LOG_INFO("Generated FreeType UI atlas and wrote cache.");
        return true;
    }

    static void uploadBatch() {
        VALKRON_CORE_ASSERT(s_data.vao != 0, "UIRenderer VAO is not initialized");
        VALKRON_CORE_ASSERT(s_data.vbo != 0, "UIRenderer VBO is not initialized");
        VALKRON_CORE_ASSERT(s_data.ebo != 0, "UIRenderer EBO is not initialized");

        glBindVertexArray(s_data.vao);

        glBindBuffer(GL_ARRAY_BUFFER, s_data.vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(s_data.pendingVertices.size() * sizeof(UIVertex)),
            s_data.pendingVertices.data(),
            GL_DYNAMIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_data.ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(s_data.pendingIndices.size() * sizeof(std::uint32_t)),
            s_data.pendingIndices.data(),
            GL_DYNAMIC_DRAW
        );

        glBindVertexArray(0);
    }

    void UIRenderer::init() {
        if (s_data.initialized) {
            LOG_WARN("UIRenderer::init called more than once. Ignoring duplicate initialization.");
            return;
        }

        s_data.shader = std::make_unique<Shader>("shaders/ui.vert", "shaders/ui.frag");

        glGenVertexArrays(1, &s_data.vao);
        glGenBuffers(1, &s_data.vbo);
        glGenBuffers(1, &s_data.ebo);

        VALKRON_CORE_ASSERT(s_data.vao != 0, "Failed to create UI VAO");
        VALKRON_CORE_ASSERT(s_data.vbo != 0, "Failed to create UI VBO");
        VALKRON_CORE_ASSERT(s_data.ebo != 0, "Failed to create UI EBO");

        glBindVertexArray(s_data.vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_data.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_data.ebo);

        const GLsizei stride = static_cast<GLsizei>(sizeof(UIVertex));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(UIVertex, position)));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(UIVertex, texCoord)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(UIVertex, color)));

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(UIVertex, sdfMode)));

        glBindVertexArray(0);

        if (!loadAtlasCache()) {
            generateAtlasFromFreeType();
        }

        if (!s_data.fontReady) {
            LOG_WARN("UIRenderer font atlas unavailable; button text rendering is disabled.");
        } else {
            const GlyphInfo& glyphP = s_data.glyphs[static_cast<std::size_t>('P')];
            LOG_INFO(
                "UIRenderer glyph sanity P: valid=" + std::to_string(glyphP.valid ? 1 : 0) +
                ", advance=" + std::to_string(glyphP.advance) +
                ", size=" + std::to_string(glyphP.width) + "x" + std::to_string(glyphP.height)
            );
            LOG_INFO("UIRenderer font texture id: " + std::to_string(s_data.fontTexture));
        }

        s_data.shader->bind();
        s_data.shader->setInt("u_FontAtlas", 0);

        s_data.initialized = true;
    }

    void UIRenderer::shutdown() {
        s_data.pendingVertices.clear();
        s_data.pendingIndices.clear();

        if (s_data.fontTexture != 0) {
            glDeleteTextures(1, &s_data.fontTexture);
            s_data.fontTexture = 0;
        }

        s_data.fontReady = false;
        s_data.glyphs = {};

        if (s_data.ebo != 0) {
            glDeleteBuffers(1, &s_data.ebo);
            s_data.ebo = 0;
        }

        if (s_data.vbo != 0) {
            glDeleteBuffers(1, &s_data.vbo);
            s_data.vbo = 0;
        }

        if (s_data.vao != 0) {
            glDeleteVertexArrays(1, &s_data.vao);
            s_data.vao = 0;
        }

        s_data.shader.reset();
        s_data.initialized = false;
    }

    void UIRenderer::submitBatch(const std::vector<UIVertex>& vertices, const std::vector<std::uint32_t>& indices) {
        if (!s_data.initialized) {
            LOG_WARN("UIRenderer::submitBatch called before UIRenderer::init");
            return;
        }

        s_data.pendingVertices = vertices;
        s_data.pendingIndices = indices;
    }

    void UIRenderer::render(int viewportWidth, int viewportHeight) {
        if (!s_data.initialized || viewportWidth <= 0 || viewportHeight <= 0) {
            return;
        }

        if (s_data.pendingVertices.empty() || s_data.pendingIndices.empty()) {
            return;
        }

        VALKRON_CORE_ASSERT(s_data.shader != nullptr, "UIRenderer shader is not initialized");

        uploadBatch();

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_FALSE);

        s_data.shader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_data.fontTexture);
        const glm::mat4 projection = glm::ortho(
            0.0f,
            static_cast<float>(viewportWidth),
            static_cast<float>(viewportHeight),
            0.0f,
            -1.0f,
            1.0f
        );
        s_data.shader->setMat4("u_Projection", glm::value_ptr(projection));

        glBindVertexArray(s_data.vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(s_data.pendingIndices.size()), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glDepthMask(GL_TRUE);

        s_data.pendingVertices.clear();
        s_data.pendingIndices.clear();
    }

    void UIRenderer::appendTextGeometry(
        const std::string& text,
        const glm::vec2& position,
        float pixelHeight,
        const glm::vec4& color,
        std::vector<UIVertex>& outVertices,
        std::vector<std::uint32_t>& outIndices
    ) {
        if (!s_data.initialized || !s_data.fontReady || text.empty() || pixelHeight <= 0.0f) {
            return;
        }

        const float scale = pixelHeight / s_data.atlasPixelHeight;
        float cursorX = position.x;
        float baselineY = position.y + s_data.ascentPx * scale;

        for (const char c : text) {
            if (c == '\n') {
                cursorX = position.x;
                baselineY += s_data.lineHeightPx * scale;
                continue;
            }

            const unsigned char code = static_cast<unsigned char>(c);
            if (code >= s_data.glyphs.size()) {
                continue;
            }

            const GlyphInfo& glyph = s_data.glyphs[code];
            if (!glyph.valid) {
                cursorX += pixelHeight * 0.3f;
                continue;
            }

            const float x0 = cursorX + glyph.xOffset * scale;
            const float y0 = baselineY + glyph.yOffset * scale;
            const float x1 = x0 + glyph.width * scale;
            const float y1 = y0 + glyph.height * scale;

            const std::uint32_t base = static_cast<std::uint32_t>(outVertices.size());
            outVertices.push_back({glm::vec3(x0, y0, 0.0f), glm::vec2(glyph.u0, glyph.v0), color, 1.0f});
            outVertices.push_back({glm::vec3(x1, y0, 0.0f), glm::vec2(glyph.u1, glyph.v0), color, 1.0f});
            outVertices.push_back({glm::vec3(x1, y1, 0.0f), glm::vec2(glyph.u1, glyph.v1), color, 1.0f});
            outVertices.push_back({glm::vec3(x0, y1, 0.0f), glm::vec2(glyph.u0, glyph.v1), color, 1.0f});

            outIndices.push_back(base + 0);
            outIndices.push_back(base + 1);
            outIndices.push_back(base + 2);
            outIndices.push_back(base + 2);
            outIndices.push_back(base + 3);
            outIndices.push_back(base + 0);

            cursorX += glyph.advance * scale;
        }
    }

    float UIRenderer::getLineHeight(float pixelHeight) {
        if (!s_data.initialized || pixelHeight <= 0.0f) {
            return pixelHeight;
        }

        const float scale = pixelHeight / s_data.atlasPixelHeight;
        return s_data.lineHeightPx * scale;
    }

    float UIRenderer::measureTextWidth(const std::string& text, float pixelHeight) {
        if (!s_data.initialized || !s_data.fontReady || text.empty() || pixelHeight <= 0.0f) {
            return 0.0f;
        }

        const float scale = pixelHeight / s_data.atlasPixelHeight;
        float width = 0.0f;

        for (const char c : text) {
            if (c == '\n') {
                break;
            }

            const unsigned char code = static_cast<unsigned char>(c);
            if (code >= s_data.glyphs.size()) {
                continue;
            }

            const GlyphInfo& glyph = s_data.glyphs[code];
            if (!glyph.valid) {
                width += pixelHeight * 0.3f;
                continue;
            }

            width += glyph.advance * scale;
        }

        return width;
    }

}
