#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <commdlg.h>
    #include <gdiplus.h>
    #include <algorithm>
    #include <cmath>
    #include <cstdint>
    #include <iterator>
    #include <string>
    #include <unordered_map>
    #include <utility>
    #include <vector>

    #pragma comment(lib, "Comdlg32.lib")
    #pragma comment(lib, "Gdiplus.lib")
#endif

using namespace geode::prelude;

namespace {
#if defined(_WIN32)
    constexpr int kMaxImageDimension = 48;
    constexpr int kMaxObjects = 2304;
    constexpr float kPixelWorldSize = 5.0f;
    constexpr float kDefaultObjectSize = 30.0f;
    constexpr BYTE kAlphaCutoff = 96;

    struct Pixel {
        BYTE r = 0;
        BYTE g = 0;
        BYTE b = 0;
        BYTE a = 0;

        uint32_t key() const {
            return (static_cast<uint32_t>(r) << 16) |
                   (static_cast<uint32_t>(g) << 8) |
                   static_cast<uint32_t>(b);
        }
    };

    struct RectRun {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 1;
        Pixel color;
    };

    ULONG_PTR g_gdiplusToken = 0;

    bool ensureGdiplus() {
        static bool initialized = [] {
            Gdiplus::GdiplusStartupInput input;
            return Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) == Gdiplus::Ok;
        }();
        return initialized;
    }

    std::wstring chooseImageFile() {
        wchar_t path[32768] = {};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = GetActiveWindow();
        dialog.lpstrFilter =
            L"Image files (PNG/JPG)\0*.png;*.jpg;*.jpeg\0"
            L"PNG files\0*.png\0"
            L"JPEG files\0*.jpg;*.jpeg\0"
            L"All files\0*.*\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = static_cast<DWORD>(std::size(path));
        dialog.lpstrTitle = L"Dmm Photo Decor - Choose an image";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        dialog.lpstrDefExt = L"png";

        if (!GetOpenFileNameW(&dialog)) {
            return {};
        }
        return path;
    }

    Pixel quantize(Gdiplus::Color const& source) {
        // A compact 64-colour palette: four levels per RGB channel.
        auto q = [](BYTE c) -> BYTE {
            int level = std::clamp((static_cast<int>(c) + 42) / 85, 0, 3);
            return static_cast<BYTE>(level * 85);
        };
        return Pixel{ q(source.GetR()), q(source.GetG()), q(source.GetB()), source.GetA() };
    }

    bool loadImagePixels(std::wstring const& path, std::vector<Pixel>& pixels,
                         int& outWidth, int& outHeight, std::string& error) {
        if (!ensureGdiplus()) {
            error = "Windows image decoder could not be initialized.";
            return false;
        }

        Gdiplus::Bitmap bitmap(path.c_str());
        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            error = "The selected PNG/JPG file could not be opened.";
            return false;
        }

        UINT sourceWidth = bitmap.GetWidth();
        UINT sourceHeight = bitmap.GetHeight();
        if (sourceWidth == 0 || sourceHeight == 0 || sourceWidth > 20000 || sourceHeight > 20000) {
            error = "The image dimensions are invalid or too large (maximum source side: 20000 px).";
            return false;
        }

        double factor = std::min(
            1.0,
            static_cast<double>(kMaxImageDimension) /
                static_cast<double>(std::max(sourceWidth, sourceHeight))
        );
        outWidth = std::max(1, static_cast<int>(std::lround(sourceWidth * factor)));
        outHeight = std::max(1, static_cast<int>(std::lround(sourceHeight * factor)));
        pixels.assign(static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight), Pixel{});

        for (int y = 0; y < outHeight; ++y) {
            UINT sourceY = std::min(sourceHeight - 1,
                static_cast<UINT>((static_cast<uint64_t>(y) * sourceHeight) / outHeight));
            for (int x = 0; x < outWidth; ++x) {
                UINT sourceX = std::min(sourceWidth - 1,
                    static_cast<UINT>((static_cast<uint64_t>(x) * sourceWidth) / outWidth));
                Gdiplus::Color source;
                if (bitmap.GetPixel(sourceX, sourceY, &source) != Gdiplus::Ok) {
                    error = "A pixel could not be read from the selected image.";
                    return false;
                }
                Pixel p = quantize(source);
                if (p.a < kAlphaCutoff) {
                    p.a = 0;
                }
                pixels[static_cast<size_t>(y) * outWidth + x] = p;
            }
        }
        return true;
    }

    std::vector<RectRun> mergePixelsIntoRectangles(std::vector<Pixel> const& pixels,
                                                   int width, int height) {
        std::vector<RectRun> rectangles;
        std::unordered_map<uint64_t, size_t> activeRuns;

        for (int y = 0; y < height; ++y) {
            std::unordered_map<uint64_t, size_t> nextRuns;
            int x = 0;
            while (x < width) {
                Pixel const& p = pixels[static_cast<size_t>(y) * width + x];
                if (p.a == 0) {
                    ++x;
                    continue;
                }

                int startX = x;
                uint32_t colorKey = p.key();
                while (x < width) {
                    Pixel const& candidate = pixels[static_cast<size_t>(y) * width + x];
                    if (candidate.a == 0 || candidate.key() != colorKey) {
                        break;
                    }
                    ++x;
                }
                int runWidth = x - startX;
                uint64_t runKey = (static_cast<uint64_t>(static_cast<uint16_t>(startX)) << 48) |
                                  (static_cast<uint64_t>(static_cast<uint16_t>(runWidth)) << 32) |
                                  static_cast<uint64_t>(colorKey);

                auto previous = activeRuns.find(runKey);
                if (previous != activeRuns.end() &&
                    rectangles[previous->second].y + rectangles[previous->second].height == y) {
                    ++rectangles[previous->second].height;
                    nextRuns.emplace(runKey, previous->second);
                } else {
                    size_t index = rectangles.size();
                    rectangles.push_back(RectRun{startX, y, runWidth, 1, p});
                    nextRuns.emplace(runKey, index);
                }
            }
            activeRuns = std::move(nextRuns);
        }
        return rectangles;
    }

    void showPhotoMessage(char const* title, std::string const& message) {
        FLAlertLayer::create(title, message.c_str(), "OK")->show();
    }

    size_t createNativeObjects(LevelEditorLayer* editorLayer,
                               std::vector<RectRun> const& rectangles,
                               int imageWidth, int imageHeight) {
        if (!editorLayer || rectangles.empty()) {
            return 0;
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint screenCenter(winSize.width * 0.5f, winSize.height * 0.5f);
        CCPoint worldCenter = editorLayer->convertToNodeSpace(screenCenter);

        float imageWorldWidth = imageWidth * kPixelWorldSize;
        float imageWorldHeight = imageHeight * kPixelWorldSize;
        float left = worldCenter.x - imageWorldWidth * 0.5f;
        float bottom = worldCenter.y - imageWorldHeight * 0.5f;
        float objectScalePerPixel = kPixelWorldSize / kDefaultObjectSize;

        size_t created = 0;
        for (auto const& rect : rectangles) {
            float rectWidth = rect.width * kPixelWorldSize;
            float rectHeight = rect.height * kPixelWorldSize;
            float x = left + (rect.x * kPixelWorldSize) + rectWidth * 0.5f;
            // Source images start at the top-left; Geometry Dash's world Y increases upward.
            float yTop = imageHeight - rect.y;
            float y = bottom + (yTop * kPixelWorldSize) - rectHeight * 0.5f;

            GameObject* object = editorLayer->createObject(1, CCPoint(x, y), false);
            if (!object) {
                continue;
            }

            object->setScaleX(rect.width * objectScalePerPixel);
            object->setScaleY(rect.height * objectScalePerPixel);

            // Keep the generated square blocks decorative and non-colliding.
            object->m_isNoTouch = true;
            object->m_isPassable = true;
            object->m_isDecoration = true;

            cocos2d::ccColor3B color(rect.color.r, rect.color.g, rect.color.b);
            if (object->m_baseColor) {
                object->m_baseColor->m_colorID = 0;
                object->m_baseColor->m_defaultColorID = 0;
                object->m_baseColor->m_usesHSV = false;
                object->m_baseColor->m_usesCustomBlend = false;
                object->m_baseColor->m_customColor = color;
            }
            object->setObjectColor(color);
            ++created;
        }
        return created;
    }

    void importPhotoIntoLevel(LevelEditorLayer* editorLayer) {
        std::wstring path = chooseImageFile();
        if (path.empty()) {
            return;
        }

        std::vector<Pixel> pixels;
        int width = 0;
        int height = 0;
        std::string error;
        if (!loadImagePixels(path, pixels, width, height, error)) {
            showPhotoMessage("Dmm Photo Decor", error);
            return;
        }

        auto rectangles = mergePixelsIntoRectangles(pixels, width, height);
        if (rectangles.empty()) {
            showPhotoMessage("Dmm Photo Decor", "No visible pixels were found. Try an image without full transparency.");
            return;
        }
        if (rectangles.size() > static_cast<size_t>(kMaxObjects)) {
            showPhotoMessage("Dmm Photo Decor", "This image needs too many objects. Try a smaller or simpler image.");
            return;
        }

        size_t created = createNativeObjects(editorLayer, rectangles, width, height);
        std::string message = "Image converted to " + std::to_string(created) +
            " native Geometry Dash objects.\n\n" +
            "The image was reduced to " + std::to_string(width) + " x " +
            std::to_string(height) + " pixels and a 64-colour palette.\n" +
            "Transparent pixels were skipped and adjacent matching pixels were merged.\n\n" +
            "Save the level in the editor to keep the generated objects.";
        showPhotoMessage("Dmm Photo Decor", message);
    }
#endif
}

class $modify(DmmPhotoDecorEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) {
            return false;
        }

#if defined(_WIN32)
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition(CCPointZero);

        auto label = CCLabelBMFont::create("PHOTO", "goldFont.fnt");
        if (label) {
            label->setScale(0.45f);
            auto button = CCMenuItemSpriteExtra::create(
                label,
                this,
                menu_selector(DmmPhotoDecorEditorUI::onDmmPhotoDecor)
            );
            if (button) {
                button->setID("dmm-photo-decor/import-button");
                button->setPosition(CCPoint(winSize.width - 62.0f, winSize.height - 42.0f));
                menu->addChild(button);
                this->addChild(menu, 10000);
            }
        }
#endif
        return true;
    }

    void onDmmPhotoDecor(CCObject*) {
#if defined(_WIN32)
        if (!m_editorLayer) {
            FLAlertLayer::create("Dmm Photo Decor", "The level editor is not available.", "OK")->show();
            return;
        }
        importPhotoIntoLevel(m_editorLayer);
#else
        FLAlertLayer::create("Dmm Photo Decor", "This prototype currently supports Windows only.", "OK")->show();
#endif
    }
};
