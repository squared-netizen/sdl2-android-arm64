#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

namespace {

constexpr int kLogicalWidth = 960;
constexpr int kLogicalHeight = 540;
constexpr int kTouchMarkerSize = 42;
constexpr std::size_t kMaximumTextBytes = 96;

constexpr const char* kFontAssetPath =
    "fonts/JetBrainsMonoNerdFont-Regular.ttf";

constexpr const char* kWavAssetPath =
    "audio/mixer-test-wav.wav";

constexpr const char* kOggAssetPath =
    "audio/mixer-test-ogg.ogg";

constexpr const char* kMp3AssetPath =
    "audio/mixer-test-mp3.mp3";

struct TextTexture {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

struct Button {
    SDL_Rect rectangle{};
    const char* label = "";
    TextTexture text;
    SDL_Color availableColor{};
    bool available = true;
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

    const SDL_Rect destination{
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

    SDL_Window* window = SDL_CreateWindow(
        "SDL Rogue Mixer Test",
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
        SDL_Quit();
        return 1;
    }

    TTF_Font* font =
        TTF_OpenFontRW(fontData, 1, 30);

    if (font == nullptr) {
        SDL_Log(
            "TTF_OpenFontRW failed: %s",
            TTF_GetError()
        );
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    constexpr int requestedMixerFlags =
        MIX_INIT_OGG |
        MIX_INIT_MP3;

    const int initializedMixerFlags =
        Mix_Init(requestedMixerFlags);

    const bool oggDecoderAvailable =
        (initializedMixerFlags & MIX_INIT_OGG) != 0;

    const bool mp3DecoderAvailable =
        (initializedMixerFlags & MIX_INIT_MP3) != 0;

    if (!oggDecoderAvailable) {
        SDL_Log(
            "Ogg decoder initialization failed: %s",
            Mix_GetError()
        );
    }

    if (!mp3DecoderAvailable) {
        SDL_Log(
            "MP3 decoder initialization failed: %s",
            Mix_GetError()
        );
    }

    const bool mixerOpen =
        Mix_OpenAudio(
            48000,
            MIX_DEFAULT_FORMAT,
            2,
            1024
        ) == 0;

    if (!mixerOpen) {
        SDL_Log(
            "Mix_OpenAudio failed: %s",
            Mix_GetError()
        );
    }

    int actualFrequency = 0;
    Uint16 actualFormat = 0;
    int actualChannels = 0;

    if (mixerOpen) {
        Mix_QuerySpec(
            &actualFrequency,
            &actualFormat,
            &actualChannels
        );

        SDL_Log(
            "Mixer opened: frequency=%d format=%u channels=%d",
            actualFrequency,
            static_cast<unsigned int>(actualFormat),
            actualChannels
        );
    }

    Mix_Chunk* wavChunk = nullptr;
    Mix_Music* oggMusic = nullptr;
    Mix_Music* mp3Music = nullptr;

    if (mixerOpen) {
        wavChunk = Mix_LoadWAV(kWavAssetPath);

        if (wavChunk == nullptr) {
            SDL_Log(
                "Mix_LoadWAV failed for %s: %s",
                kWavAssetPath,
                Mix_GetError()
            );
        }

        if (oggDecoderAvailable) {
            oggMusic = Mix_LoadMUS(kOggAssetPath);

            if (oggMusic == nullptr) {
                SDL_Log(
                    "Mix_LoadMUS failed for %s: %s",
                    kOggAssetPath,
                    Mix_GetError()
                );
            }
        }

        if (mp3DecoderAvailable) {
            mp3Music = Mix_LoadMUS(kMp3AssetPath);

            if (mp3Music == nullptr) {
                SDL_Log(
                    "Mix_LoadMUS failed for %s: %s",
                    kMp3AssetPath,
                    Mix_GetError()
                );
            }
        }
    }

    Mix_Volume(-1, MIX_MAX_VOLUME / 2);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 2);

    constexpr int buttonY = 84;
    constexpr int buttonWidth = 172;
    constexpr int buttonHeight = 78;
    constexpr int buttonGap = 14;

    std::array<Button, 5> buttons{{
        {
            SDL_Rect{24, buttonY, buttonWidth, buttonHeight},
            "KEYBOARD",
            {},
            SDL_Color{40, 190, 190, 255},
            true
        },
        {
            SDL_Rect{
                24 + (buttonWidth + buttonGap),
                buttonY,
                buttonWidth,
                buttonHeight
            },
            "WAV SFX",
            {},
            SDL_Color{80, 190, 100, 255},
            mixerOpen && wavChunk != nullptr
        },
        {
            SDL_Rect{
                24 + 2 * (buttonWidth + buttonGap),
                buttonY,
                buttonWidth,
                buttonHeight
            },
            "OGG MUSIC",
            {},
            SDL_Color{75, 125, 235, 255},
            mixerOpen &&
                oggDecoderAvailable &&
                oggMusic != nullptr
        },
        {
            SDL_Rect{
                24 + 3 * (buttonWidth + buttonGap),
                buttonY,
                buttonWidth,
                buttonHeight
            },
            "MP3 MUSIC",
            {},
            SDL_Color{175, 90, 220, 255},
            mixerOpen &&
                mp3DecoderAvailable &&
                mp3Music != nullptr
        },
        {
            SDL_Rect{
                24 + 4 * (buttonWidth + buttonGap),
                buttonY,
                buttonWidth,
                buttonHeight
            },
            "STOP",
            {},
            SDL_Color{210, 90, 70, 255},
            mixerOpen
        }
    }};

    const SDL_Rect headingArea{
        24,
        16,
        kLogicalWidth - 48,
        52
    };

    const SDL_Rect statusArea{
        24,
        182,
        kLogicalWidth - 48,
        58
    };

    const SDL_Rect inputArea{
        24,
        kLogicalHeight - 76,
        kLogicalWidth - 48,
        52
    };

    SDL_Rect textInputPosition = inputArea;
    SDL_SetTextInputRect(&textInputPosition);

    const std::string headingText =
        "SDL2_mixer  WAV  OGG  MP3  "
        "\xEF\x80\xA8";

    TextTexture headingTexture;
    TextTexture statusTexture;
    TextTexture inputTexture;

    const SDL_Color lightText{
        238,
        238,
        246,
        255
    };

    const SDL_Color inputTextColor{
        80,
        220,
        140,
        255
    };

    std::string statusText;

    if (!mixerOpen) {
        statusText = "MIXER FAILED: " + std::string(Mix_GetError());
    } else if (wavChunk == nullptr ||
               oggMusic == nullptr ||
               mp3Music == nullptr) {
        statusText = "One or more test assets failed to load";
    } else {
        statusText = "READY: tap WAV, OGG, and MP3";
    }

    std::string textBuffer;
    bool interfaceTexturesDirty = true;
    bool inputTextureDirty = true;
    bool statusTextureDirty = true;

    auto set_status = [&](const std::string& message) {
        statusText = message;
        statusTextureDirty = true;
        SDL_Log("%s", message.c_str());
    };

    auto perform_button_action = [&](std::size_t index) {
        if (index >= buttons.size()) {
            return;
        }

        if (!buttons[index].available) {
            set_status(
                std::string(buttons[index].label) +
                " is unavailable"
            );
            return;
        }

        switch (index) {
            case 0:
                toggle_text_input();
                set_status(
                    SDL_IsTextInputActive() == SDL_TRUE
                        ? "Software keyboard active"
                        : "Software keyboard stopped"
                );
                break;

            case 1:
                if (Mix_PlayChannel(-1, wavChunk, 0) < 0) {
                    set_status(
                        "WAV failed: " +
                        std::string(Mix_GetError())
                    );
                } else {
                    set_status(
                        "WAV OK: 440 Hz channel sound"
                    );
                }
                break;

            case 2:
                Mix_HaltMusic();

                if (Mix_PlayMusic(oggMusic, 0) != 0) {
                    set_status(
                        "OGG failed: " +
                        std::string(Mix_GetError())
                    );
                } else {
                    set_status(
                        "OGG OK: 554 Hz music"
                    );
                }
                break;

            case 3:
                Mix_HaltMusic();

                if (Mix_PlayMusic(mp3Music, 0) != 0) {
                    set_status(
                        "MP3 failed: " +
                        std::string(Mix_GetError())
                    );
                } else {
                    set_status(
                        "MP3 OK: 659 Hz music"
                    );
                }
                break;

            case 4:
                Mix_HaltChannel(-1);
                Mix_HaltMusic();
                set_status("Playback stopped");
                break;

            default:
                break;
        }
    };

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

                    if (mixerOpen) {
                        Mix_Pause(-1);
                        Mix_PauseMusic();
                    }
                    break;

                case SDL_APP_DIDENTERFOREGROUND:
                    backgrounded = false;

                    if (mixerOpen) {
                        Mix_Resume(-1);
                        Mix_ResumeMusic();
                    }
                    break;

                case SDL_RENDER_DEVICE_RESET:
                case SDL_RENDER_TARGETS_RESET:
                    interfaceTexturesDirty = true;
                    inputTextureDirty = true;
                    statusTextureDirty = true;
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
                        for (std::size_t index = 0;
                             index < buttons.size();
                             ++index) {
                            if (point_inside(
                                    touchX,
                                    touchY,
                                    buttons[index].rectangle
                                )) {
                                perform_button_action(index);
                                break;
                            }
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

                    inputTextureDirty = true;
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        erase_last_utf8_character(textBuffer);
                        inputTextureDirty = true;
                        break;
                    }

                    if (event.key.keysym.sym == SDLK_RETURN ||
                        event.key.keysym.sym == SDLK_KP_ENTER) {
                        SDL_StopTextInput();
                        break;
                    }

                    if (event.key.keysym.sym == SDLK_1) {
                        perform_button_action(1);
                        break;
                    }

                    if (event.key.keysym.sym == SDLK_2) {
                        perform_button_action(2);
                        break;
                    }

                    if (event.key.keysym.sym == SDLK_3) {
                        perform_button_action(3);
                        break;
                    }

                    if (event.key.keysym.sym == SDLK_0) {
                        perform_button_action(4);
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

        if (interfaceTexturesDirty) {
            create_text_texture(
                renderer,
                font,
                headingText,
                lightText,
                headingTexture
            );

            for (Button& button : buttons) {
                create_text_texture(
                    renderer,
                    font,
                    button.label,
                    lightText,
                    button.text
                );
            }

            interfaceTexturesDirty = false;
            statusTextureDirty = true;
            inputTextureDirty = true;
        }

        if (statusTextureDirty) {
            create_text_texture(
                renderer,
                font,
                statusText,
                lightText,
                statusTexture
            );
            statusTextureDirty = false;
        }

        if (inputTextureDirty) {
            const std::string displayedText =
                textBuffer.empty()
                    ? "Keyboard test: tap KEYBOARD and type"
                    : textBuffer;

            create_text_texture(
                renderer,
                font,
                displayedText,
                inputTextColor,
                inputTexture
            );
            inputTextureDirty = false;
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

        SDL_SetRenderDrawColor(renderer, 24, 24, 38, 255);
        SDL_RenderFillRect(renderer, &headingArea);
        SDL_RenderFillRect(renderer, &statusArea);
        SDL_RenderFillRect(renderer, &inputArea);

        draw_text_centered(
            renderer,
            headingTexture,
            headingArea
        );

        draw_text_centered(
            renderer,
            statusTexture,
            statusArea
        );

        draw_text_centered(
            renderer,
            inputTexture,
            inputArea
        );

        for (std::size_t index = 0;
             index < buttons.size();
             ++index) {
            Button& button = buttons[index];

            if (!button.available) {
                SDL_SetRenderDrawColor(
                    renderer,
                    120,
                    45,
                    45,
                    255
                );
            } else if (
                index == 0 &&
                SDL_IsTextInputActive() == SDL_TRUE
            ) {
                SDL_SetRenderDrawColor(
                    renderer,
                    190,
                    70,
                    220,
                    255
                );
            } else {
                SDL_SetRenderDrawColor(
                    renderer,
                    button.availableColor.r,
                    button.availableColor.g,
                    button.availableColor.b,
                    255
                );
            }

            SDL_RenderFillRect(
                renderer,
                &button.rectangle
            );

            draw_text_centered(
                renderer,
                button.text,
                button.rectangle
            );
        }

        SDL_Rect centerTile{
            kLogicalWidth / 2 - 32,
            320,
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

    if (mixerOpen) {
        Mix_HaltChannel(-1);
        Mix_HaltMusic();
    }

    if (wavChunk != nullptr) {
        Mix_FreeChunk(wavChunk);
    }

    if (oggMusic != nullptr) {
        Mix_FreeMusic(oggMusic);
    }

    if (mp3Music != nullptr) {
        Mix_FreeMusic(mp3Music);
    }

    if (mixerOpen) {
        Mix_CloseAudio();
    }

    Mix_Quit();

    destroy_text_texture(inputTexture);
    destroy_text_texture(statusTexture);
    destroy_text_texture(headingTexture);

    for (Button& button : buttons) {
        destroy_text_texture(button.text);
    }

    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
