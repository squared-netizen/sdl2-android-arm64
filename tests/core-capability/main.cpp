#include <SDL.h>

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
        SDL_Log("Cannot play tone: audio device is unavailable");
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
        SDL_Log("SDL_QueueAudio failed: %s", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(audioDevice, 0);
    SDL_Log("Queued test tone");

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
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Log(
        "Screen keyboard support: %s",
        SDL_HasScreenKeyboardSupport() == SDL_TRUE ? "yes" : "no"
    );

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

    if (audioAvailable) {
        SDL_Log(
            "Audio opened: frequency=%d format=%u channels=%u",
            obtainedAudio.freq,
            static_cast<unsigned int>(obtainedAudio.format),
            static_cast<unsigned int>(obtainedAudio.channels)
        );
    } else {
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
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());

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
        SDL_Log(
            "Accelerated renderer failed, trying software: %s",
            SDL_GetError()
        );

        renderer = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_SOFTWARE
        );
    }

    if (renderer == nullptr) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());

        SDL_DestroyWindow(window);

        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
        }

        SDL_Quit();
        return 1;
    }

    if (SDL_RenderSetLogicalSize(
            renderer,
            kLogicalWidth,
            kLogicalHeight
        ) != 0) {
        SDL_Log(
            "SDL_RenderSetLogicalSize failed: %s",
            SDL_GetError()
        );
    }

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

    SDL_Rect textInputPosition{
        24,
        kLogicalHeight - 72,
        kLogicalWidth - 48,
        48
    };

    SDL_SetTextInputRect(&textInputPosition);

    bool running = true;
    bool backgrounded = false;
    std::uint8_t pulse = 0;

    std::map<SDL_FingerID, SDL_FPoint> fingers;
    std::string textBuffer;

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
                    SDL_Log("Renderer resources must be recreated");
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

                    SDL_Log(
                        "SDL_TEXTINPUT: %s",
                        event.text.text
                    );
                    break;

                case SDL_TEXTEDITING:
                    SDL_Log(
                        "SDL_TEXTEDITING: %s",
                        event.edit.text
                    );
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        erase_last_utf8_character(textBuffer);
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

        ++pulse;

        SDL_SetRenderDrawColor(
            renderer,
            8,
            static_cast<Uint8>(pulse / 3),
            24,
            255
        );
        SDL_RenderClear(renderer);

        SDL_Rect centerTile{
            kLogicalWidth / 2 - 32,
            kLogicalHeight / 2 - 32,
            64,
            64
        };

        SDL_SetRenderDrawColor(renderer, 80, 220, 140, 255);
        SDL_RenderFillRect(renderer, &centerTile);

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

        SDL_SetRenderDrawColor(renderer, 35, 35, 45, 255);
        SDL_RenderFillRect(renderer, &textInputPosition);

        const int maximumTextBarWidth = textInputPosition.w;

        const int textBarWidth = std::min(
            static_cast<int>(textBuffer.size()) * 14,
            maximumTextBarWidth
        );

        if (textBarWidth > 0) {
            SDL_Rect textReceivedBar{
                textInputPosition.x,
                textInputPosition.y,
                textBarWidth,
                textInputPosition.h
            };

            SDL_SetRenderDrawColor(renderer, 80, 220, 140, 255);
            SDL_RenderFillRect(renderer, &textReceivedBar);
        }

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

    if (audioDevice != 0) {
        SDL_ClearQueuedAudio(audioDevice);
        SDL_CloseAudioDevice(audioDevice);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
