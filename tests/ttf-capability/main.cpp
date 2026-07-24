#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int kLogicalWidth = 960;
constexpr int kLogicalHeight = 540;
constexpr int kTouchMarkerSize = 48;

constexpr int kKeyboardButtonX = 24;
constexpr int kKeyboardButtonY = 24;
constexpr int kKeyboardButtonWidth = 216;
constexpr int kKeyboardButtonHeight = 96;

constexpr int kAudioButtonX = 720;
constexpr int kAudioButtonY = 24;
constexpr int kAudioButtonWidth = 216;
constexpr int kAudioButtonHeight = 96;

constexpr int kAudioFrequency = 48000;
constexpr int kToneFrequency = 440;
constexpr int kToneDurationMs = 350;
constexpr double kPi = 3.14159265358979323846;

constexpr std::size_t kMaximumTextBytes = 96;

constexpr const char* kFontAssetPath =
    "fonts/JetBrainsMonoNerdFont-Regular.ttf";

struct TextTexture {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

void save_game()
{
    SDL_Log("Lifecycle save point");
}

bool point_inside(
    float x,
    float y,
    const SDL_Rect& rectangle
)
{
    return
        x >= static_cast<float>(rectangle.x) &&
        x < static_cast<float>(rectangle.x + rectangle.w) &&
        y >= static_cast<float>(rectangle.y) &&
        y < static_cast<float>(rectangle.y + rectangle.h);
}

void destroy_text_texture(TextTexture& textTexture)
{
    if (textTexture.texture != nullptr) {
        SDL_DestroyTexture(textTexture.texture);
    }

    textTexture.texture = nullptr;
    textTexture.width = 0;
    textTexture.height = 0;
}

bool create_text_texture(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::string& text,
    SDL_Color color,
    TextTexture& destination
)
{
    destroy_text_texture(destination);

    if (text.empty()) {
        return true;
    }

    SDL_Surface* surface =
        TTF_RenderUTF8_Blended(
            font,
            text.c_str(),
            color
        );

    if (surface == nullptr) {
        SDL_Log(
            "TTF_RenderUTF8_Blended failed: %s",
            TTF_GetError()
        );
        return false;
    }

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface
        );

    if (texture == nullptr) {
        SDL_Log(
            "SDL_CreateTextureFromSurface failed: %s",
            SDL_GetError()
        );

        SDL_FreeSurface(surface);
        return false;
    }

    destination.texture = texture;
    destination.width = surface->w;
    destination.height = surface->h;

    SDL_FreeSurface(surface);
    return true;
}

void draw_text_centered(
    SDL_Renderer* renderer,
    const TextTexture& textTexture,
    const SDL_Rect& area
)
{
    if (textTexture.texture == nullptr ||
        textTexture.width <= 0 ||
        textTexture.height <= 0) {
        return;
    }

    const float horizontalScale =
        static_cast<float>(area.w) /
        static_cast<float>(textTexture.width);

    const float verticalScale =
        static_cast<float>(area.h) /
        static_cast<float>(textTexture.height);

    const float scale = std::min(
        1.0F,
        std::min(horizontalScale, verticalScale)
    );

    const int destinationWidth =
        static_cast<int>(
            static_cast<float>(textTexture.width) * scale
        );

    const int destinationHeight =
        static_cast<int>(
            static_cast<float>(textTexture.height) * scale
        );

    SDL_Rect destination{
        area.x + (area.w - destinationWidth) / 2,
        area.y + (area.h - destinationHeight) / 2,
        destinationWidth,
        destinationHeight
    };

    SDL_RenderCopy(
        renderer,
        textTexture.texture,
        nullptr,
        &destination
    );
}

void erase_last_utf8_character(std::string& text)
{
    if (text.empty()) {
        return;
    }

    std::size_t characterStart = text.size() - 1;

    while (characterStart > 0) {
        const auto byte =
            static_cast<unsigned char>(text[characterStart]);

        if ((byte & 0xC0U) != 0x80U) {
            break;
        }

        --characterStart;
    }

    text.erase(characterStart);
}

void toggle_text_input()
{
    if (SDL_IsTextInputActive() == SDL_TRUE) {
        SDL_StopTextInput();
        SDL_Log("Text input stopped");
    } else {
        SDL_StartTextInput();
        SDL_Log("Text input started");
    }
}

bool play_test_tone(
    SDL_AudioDeviceID audioDevice,
    const SDL_AudioSpec& audioSpec
)
{
    if (audioDevice == 0) {
        SDL_Log("Cannot play tone: audio device unavailable");
        return false;
    }

    if (audioSpec.format != AUDIO_S16SYS ||
        audioSpec.channels != 1) {
        SDL_Log(
            "Unexpected audio format: format=%u channels=%u",
            static_cast<unsigned int>(audioSpec.format),
            static_cast<unsigned int>(audioSpec.channels)
        );
        return false;
    }

    const int sampleCount =
        audioSpec.freq * kToneDurationMs / 1000;

    std::vector<Sint16> samples(
        static_cast<std::size_t>(sampleCount)
    );

    for (int index = 0; index < sampleCount; ++index) {
        const double time =
            static_cast<double>(index) /
            static_cast<double>(audioSpec.freq);

        const double sample =
            std::sin(
                2.0 *
                kPi *
                static_cast<double>(kToneFrequency) *
                time
            );

        samples[static_cast<std::size_t>(index)] =
            static_cast<Sint16>(sample * 9000.0);
    }

    SDL_ClearQueuedAudio(audioDevice);

    if (SDL_QueueAudio(
            audioDevice,
            samples.data(),
            static_cast<Uint32>(
                samples.size() * sizeof(Sint16)
            )
        ) != 0) {
        SDL_Log(
            "SDL_QueueAudio failed: %s",
            SDL_GetError()
        );
        return false;
    }

    SDL_PauseAudioDevice(audioDevice, 0);
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    constexpr Uint32 initFlags =
        SDL_INIT_VIDEO |
        SDL_INIT_AUDIO |
        SDL_INIT_GAMECONTROLLER;

    if (SDL_Init(initFlags) != 0) {
        std::fprintf(
            stderr,
            "SDL_Init failed: %s\n",
            SDL_GetError()
        );
        return 1;
    }

    SDL_AudioSpec desiredAudio{};
    desiredAudio.freq = kAudioFrequency;
    desiredAudio.format = AUDIO_S16SYS;
    desiredAudio.channels = 1;
    desiredAudio.samples = 1024;
    desiredAudio.callback = nullptr;

    SDL_AudioSpec obtainedAudio{};

    const SDL_AudioDeviceID audioDevice =
        SDL_OpenAudioDevice(
            nullptr,
            0,
            &desiredAudio,
            &obtainedAudio,
            0
        );

    const bool audioAvailable = audioDevice != 0;

    if (!audioAvailable) {
        SDL_Log(
            "SDL_OpenAudioDevice failed: %s",
            SDL_GetError()
        );
    }

    SDL_Window* window = SDL_CreateWindow(
        "SDL Rogue",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        kLogicalWidth,
        kLogicalHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (window == nullptr) {
        SDL_Log(
            "SDL_CreateWindow failed: %s",
            SDL_GetError()
        );

        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
        }

        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_SOFTWARE
        );
    }

    if (renderer == nullptr) {
        SDL_Log(
            "SDL_CreateRenderer failed: %s",
            SDL_GetError()
        );

        SDL_DestroyWindow(window);

        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
        }

        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(
        renderer,
        kLogicalWidth,
        kLogicalHeight
    );

    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
        }

        SDL_Quit();
        return 1;
    }

    SDL_RWops* fontData =
        SDL_RWFromFile(kFontAssetPath, "rb");

    if (fontData == nullptr) {
        SDL_Log(
            "SDL_RWFromFile failed for %s: %s",
            kFontAssetPath,
            SDL_GetError()
        );

        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
        }

        SDL_Quit();
        return 1;
    }

    TTF_Font* font =
        TTF_OpenFontRW(fontData, 1, 34);

    if (font == nullptr) {
        SDL_Log(
            "TTF_OpenFontRW failed: %s",
            TTF_GetError()
        );

        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
        }

        SDL_Quit();
        return 1;
    }

    const bool folderGlyphAvailable =
        TTF_GlyphIsProvided32(font, 0xE5FF) != 0;

    const bool fileGlyphAvailable =
        TTF_GlyphIsProvided32(font, 0xF15B) != 0;

    const bool speakerGlyphAvailable =
        TTF_GlyphIsProvided32(font, 0xF028) != 0;

    SDL_Log(
        "Nerd glyphs: folder=%s file=%s speaker=%s",
        folderGlyphAvailable ? "yes" : "no",
        fileGlyphAvailable ? "yes" : "no",
        speakerGlyphAvailable ? "yes" : "no"
    );

    const SDL_Rect keyboardButton{
        kKeyboardButtonX,
        kKeyboardButtonY,
        kKeyboardButtonWidth,
        kKeyboardButtonHeight
    };

    const SDL_Rect audioButton{
        kAudioButtonX,
        kAudioButtonY,
        kAudioButtonWidth,
        kAudioButtonHeight
    };

    const SDL_Rect headingArea{
        24,
        140,
        kLogicalWidth - 48,
        80
    };

    const SDL_Rect inputArea{
        24,
        kLogicalHeight - 80,
        kLogicalWidth - 48,
        56
    };

    SDL_Rect textInputPosition = inputArea;
    SDL_SetTextInputRect(&textInputPosition);

    const std::string headingText =
        "SDL2_ttf works  "
        "\xEE\x97\xBF  "
        "\xEF\x85\x9B  "
        "\xEF\x80\xA8";

    TextTexture headingTexture;
    TextTexture inputTexture;

    const SDL_Color headingColor{
        235,
        235,
        245,
        255
    };

    const SDL_Color inputColor{
        80,
        220,
        140,
        255
    };

    create_text_texture(
        renderer,
        font,
        headingText,
        headingColor,
        headingTexture
    );

    std::string textBuffer;
    bool textTextureDirty = true;

    bool running = true;
    bool backgrounded = false;
    std::uint8_t pulse = 0;

    std::map<SDL_FingerID, SDL_FPoint> fingers;

    while (running) {
        SDL_Event event{};

        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_APP_WILLENTERBACKGROUND:
                    save_game();
                    fingers.clear();
                    backgrounded = true;

                    if (SDL_IsTextInputActive() == SDL_TRUE) {
                        SDL_StopTextInput();
                    }

                    if (audioDevice != 0) {
                        SDL_PauseAudioDevice(audioDevice, 1);
                    }
                    break;

                case SDL_APP_DIDENTERFOREGROUND:
                    backgrounded = false;

                    if (audioDevice != 0) {
                        SDL_PauseAudioDevice(audioDevice, 0);
                    }
                    break;

                case SDL_RENDER_DEVICE_RESET:
                case SDL_RENDER_TARGETS_RESET:
                    destroy_text_texture(headingTexture);
                    destroy_text_texture(inputTexture);

                    create_text_texture(
                        renderer,
                        font,
                        headingText,
                        headingColor,
                        headingTexture
                    );

                    textTextureDirty = true;
                    break;

                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION: {
                    const float touchX =
                        event.tfinger.x * kLogicalWidth;

                    const float touchY =
                        event.tfinger.y * kLogicalHeight;

                    fingers[event.tfinger.fingerId] =
                        SDL_FPoint{touchX, touchY};

                    if (event.type == SDL_FINGERDOWN) {
                        if (point_inside(
                                touchX,
                                touchY,
                                keyboardButton
                            )) {
                            toggle_text_input();
                        }

                        if (point_inside(
                                touchX,
                                touchY,
                                audioButton
                            )) {
                            play_test_tone(
                                audioDevice,
                                obtainedAudio
                            );
                        }
                    }
                    break;
                }

                case SDL_FINGERUP:
                    fingers.erase(event.tfinger.fingerId);
                    break;

                case SDL_TEXTINPUT:
                    textBuffer += event.text.text;

                    if (textBuffer.size() > kMaximumTextBytes) {
                        textBuffer.erase(kMaximumTextBytes);
                    }

                    textTextureDirty = true;
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        erase_last_utf8_character(textBuffer);
                        textTextureDirty = true;
                        break;
                    }

                    if (event.key.keysym.sym == SDLK_RETURN ||
                        event.key.keysym.sym == SDLK_KP_ENTER) {
                        SDL_StopTextInput();
                        break;
                    }

                    if (event.key.keysym.sym == SDLK_AC_BACK ||
                        event.key.keysym.sym == SDLK_ESCAPE) {
                        if (SDL_IsTextInputActive() == SDL_TRUE) {
                            SDL_StopTextInput();
                        } else {
                            running = false;
                        }
                    }
                    break;

                default:
                    break;
            }
        }

        if (backgrounded) {
            SDL_Delay(50);
            continue;
        }

        if (textTextureDirty) {
            const std::string displayedText =
                textBuffer.empty()
                    ? "Tap the teal box and type"
                    : textBuffer;

            create_text_texture(
                renderer,
                font,
                displayedText,
                inputColor,
                inputTexture
            );

            textTextureDirty = false;
        }

        ++pulse;

        SDL_SetRenderDrawColor(
            renderer,
            8,
            static_cast<Uint8>(pulse / 3),
            24,
            255
        );
        SDL_RenderClear(renderer);

        if (SDL_IsTextInputActive() == SDL_TRUE) {
            SDL_SetRenderDrawColor(renderer, 190, 70, 220, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 40, 190, 190, 255);
        }

        SDL_RenderFillRect(renderer, &keyboardButton);

        if (audioAvailable) {
            SDL_SetRenderDrawColor(renderer, 60, 120, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
        }

        SDL_RenderFillRect(renderer, &audioButton);

        SDL_SetRenderDrawColor(renderer, 24, 24, 38, 255);
        SDL_RenderFillRect(renderer, &headingArea);
        SDL_RenderFillRect(renderer, &inputArea);

        draw_text_centered(
            renderer,
            headingTexture,
            headingArea
        );

        draw_text_centered(
            renderer,
            inputTexture,
            inputArea
        );

        SDL_Rect centerTile{
            kLogicalWidth / 2 - 32,
            kLogicalHeight / 2 - 32,
            64,
            64
        };

        SDL_SetRenderDrawColor(renderer, 80, 220, 140, 255);
        SDL_RenderFillRect(renderer, &centerTile);

        for (const auto& finger : fingers) {
            const SDL_FPoint& position = finger.second;

            SDL_Rect marker{
                static_cast<int>(position.x) -
                    kTouchMarkerSize / 2,
                static_cast<int>(position.y) -
                    kTouchMarkerSize / 2,
                kTouchMarkerSize,
                kTouchMarkerSize
            };

            SDL_SetRenderDrawColor(renderer, 255, 190, 40, 255);
            SDL_RenderFillRect(renderer, &marker);
        }

        SDL_RenderPresent(renderer);
    }

    save_game();

    if (SDL_IsTextInputActive() == SDL_TRUE) {
        SDL_StopTextInput();
    }

    destroy_text_texture(inputTexture);
    destroy_text_texture(headingTexture);

    TTF_CloseFont(font);
    TTF_Quit();

    if (audioDevice != 0) {
        SDL_ClearQueuedAudio(audioDevice);
        SDL_CloseAudioDevice(audioDevice);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
