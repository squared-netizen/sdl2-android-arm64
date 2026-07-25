#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
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

struct ImageTest {
    const char* label = "";
    const char* assetPath = "";
    SDL_Rect destination{};
    SDL_Texture* texture = nullptr;
    bool loaded = false;
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

void destroy_image(ImageTest& image)
{
    if (image.texture != nullptr) {
        SDL_DestroyTexture(image.texture);
    }

    image.texture = nullptr;
    image.loaded = false;
}

bool load_image(
    SDL_Renderer* renderer,
    ImageTest& image
)
{
    destroy_image(image);

    SDL_Surface* surface = IMG_Load(image.assetPath);

    if (surface == nullptr) {
        SDL_Log(
            "IMG_Load failed for %s: %s",
            image.assetPath,
            IMG_GetError()
        );
        return false;
    }

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    if (texture == nullptr) {
        SDL_Log(
            "SDL_CreateTextureFromSurface failed for %s: %s",
            image.assetPath,
            SDL_GetError()
        );
        SDL_FreeSurface(surface);
        return false;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    image.texture = texture;
    image.loaded = true;

    SDL_Log(
        "%s loaded: %dx%d",
        image.label,
        surface->w,
        surface->h
    );

    SDL_FreeSurface(surface);
    return true;
}

std::size_t load_all_images(
    SDL_Renderer* renderer,
    std::array<ImageTest, 4>& images
)
{
    std::size_t loadedCount = 0;

    for (ImageTest& image : images) {
        if (load_image(renderer, image)) {
            ++loadedCount;
        }
    }

    return loadedCount;
}

struct LoopbackResult {
    bool success = false;
    std::string message;
};

bool send_all(
    TCPsocket socket,
    const std::string& message
)
{
    int sentBytes = 0;
    const int messageBytes =
        static_cast<int>(message.size());

    while (sentBytes < messageBytes) {
        const int sentNow =
            SDLNet_TCP_Send(
                socket,
                message.data() + sentBytes,
                messageBytes - sentBytes
            );

        if (sentNow <= 0) {
            return false;
        }

        sentBytes += sentNow;
    }

    return true;
}

bool receive_exact(
    TCPsocket socket,
    std::size_t expectedBytes,
    std::string& received
)
{
    received.clear();
    received.reserve(expectedBytes);

    SDLNet_SocketSet socketSet =
        SDLNet_AllocSocketSet(1);

    if (socketSet == nullptr) {
        return false;
    }

    if (SDLNet_TCP_AddSocket(socketSet, socket) < 0) {
        SDLNet_FreeSocketSet(socketSet);
        return false;
    }

    std::array<char, 256> buffer{};
    bool success = true;

    while (received.size() < expectedBytes) {
        const int ready =
            SDLNet_CheckSockets(socketSet, 1000);

        if (ready <= 0 ||
            SDLNet_SocketReady(socket) == 0) {
            success = false;
            break;
        }

        const std::size_t remaining =
            expectedBytes - received.size();

        const int requested =
            static_cast<int>(
                std::min(remaining, buffer.size())
            );

        const int receivedNow =
            SDLNet_TCP_Recv(
                socket,
                buffer.data(),
                requested
            );

        if (receivedNow <= 0) {
            success = false;
            break;
        }

        received.append(
            buffer.data(),
            static_cast<std::size_t>(receivedNow)
        );
    }

    SDLNet_TCP_DelSocket(socketSet, socket);
    SDLNet_FreeSocketSet(socketSet);

    return success && received.size() == expectedBytes;
}

LoopbackResult run_tcp_loopback()
{
    constexpr std::array<Uint16, 8> candidatePorts{{
        45832,
        45833,
        45834,
        45835,
        45836,
        45837,
        45838,
        45839
    }};

    TCPsocket server = nullptr;
    TCPsocket client = nullptr;
    TCPsocket accepted = nullptr;
    Uint16 selectedPort = 0;

    auto close_sockets = [&]() {
        if (accepted != nullptr) {
            SDLNet_TCP_Close(accepted);
            accepted = nullptr;
        }

        if (client != nullptr) {
            SDLNet_TCP_Close(client);
            client = nullptr;
        }

        if (server != nullptr) {
            SDLNet_TCP_Close(server);
            server = nullptr;
        }
    };

    for (const Uint16 port : candidatePorts) {
        IPaddress serverAddress{};

        if (SDLNet_ResolveHost(
                &serverAddress,
                nullptr,
                port
            ) != 0) {
            continue;
        }

        server =
            SDLNet_TCP_OpenServer(&serverAddress);

        if (server != nullptr) {
            selectedPort = port;
            break;
        }
    }

    if (server == nullptr) {
        return {
            false,
            "could not open a localhost server: " +
                std::string(SDLNet_GetError())
        };
    }

    IPaddress clientAddress{};

    if (SDLNet_ResolveHost(
            &clientAddress,
            "127.0.0.1",
            selectedPort
        ) != 0) {
        const std::string error = SDLNet_GetError();
        close_sockets();
        return {
            false,
            "could not resolve 127.0.0.1: " + error
        };
    }

    client =
        SDLNet_TCP_OpenClient(&clientAddress);

    if (client == nullptr) {
        const std::string error = SDLNet_GetError();
        close_sockets();
        return {
            false,
            "could not open localhost client: " + error
        };
    }

    for (int attempt = 0;
         attempt < 200 && accepted == nullptr;
         ++attempt) {
        accepted = SDLNet_TCP_Accept(server);

        if (accepted == nullptr) {
            SDL_Delay(5);
        }
    }

    if (accepted == nullptr) {
        close_sockets();
        return {
            false,
            "localhost server did not accept the client"
        };
    }

    const std::string request =
        "SDL2_NET_LOOPBACK_REQUEST";

    const std::string response =
        "SDL2_NET_LOOPBACK_REPLY";

    std::string receivedRequest;
    std::string receivedResponse;

    if (!send_all(client, request)) {
        const std::string error = SDLNet_GetError();
        close_sockets();
        return {
            false,
            "client send failed: " + error
        };
    }

    if (!receive_exact(
            accepted,
            request.size(),
            receivedRequest
        ) ||
        receivedRequest != request) {
        const std::string error = SDLNet_GetError();
        close_sockets();
        return {
            false,
            "server receive failed: " + error
        };
    }

    if (!send_all(accepted, response)) {
        const std::string error = SDLNet_GetError();
        close_sockets();
        return {
            false,
            "server send failed: " + error
        };
    }

    if (!receive_exact(
            client,
            response.size(),
            receivedResponse
        ) ||
        receivedResponse != response) {
        const std::string error = SDLNet_GetError();
        close_sockets();
        return {
            false,
            "client receive failed: " + error
        };
    }

    close_sockets();

    return {
        true,
        "TCP loopback request/reply passed on port " +
            std::to_string(selectedPort)
    };
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
        "SDL Rogue Image Test",
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
        SDL_Log(
            "SDL_CreateRenderer failed: %s",
            SDL_GetError()
        );
        SDL_DestroyWindow(window);
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
        TTF_OpenFontRW(fontData, 1, 25);

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

    const bool netInitialized =
        SDLNet_Init() == 0;

    LoopbackResult loopbackResult;

    if (netInitialized) {
        loopbackResult = run_tcp_loopback();
    } else {
        loopbackResult = {
            false,
            "SDLNet_Init failed: " +
                std::string(SDLNet_GetError())
        };
    }

    SDL_Log(
        "SDL2_net test: %s",
        loopbackResult.message.c_str()
    );

    constexpr int requestedImageFlags =
        IMG_INIT_JPG |
        IMG_INIT_PNG;

    const int initializedImageFlags =
        IMG_Init(requestedImageFlags);

    const bool pngDecoderAvailable =
        (initializedImageFlags & IMG_INIT_PNG) != 0;

    const bool jpegDecoderAvailable =
        (initializedImageFlags & IMG_INIT_JPG) != 0;

    if (!pngDecoderAvailable) {
        SDL_Log(
            "PNG decoder initialization failed: %s",
            IMG_GetError()
        );
    }

    if (!jpegDecoderAvailable) {
        SDL_Log(
            "JPEG decoder initialization failed: %s",
            IMG_GetError()
        );
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

    constexpr int buttonY = 66;
    constexpr int buttonWidth = 172;
    constexpr int buttonHeight = 62;
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

    constexpr int imageY = 240;
    constexpr int imageWidth = 192;
    constexpr int imageHeight = 128;
    constexpr int imageGap = 48;
    constexpr int firstImageX = 24;

    std::array<ImageTest, 4> images{{
        {
            "PNG",
            "images/image-test-png.png",
            SDL_Rect{
                firstImageX,
                imageY,
                imageWidth,
                imageHeight
            }
        },
        {
            "JPEG",
            "images/image-test-jpeg.jpg",
            SDL_Rect{
                firstImageX + imageWidth + imageGap,
                imageY,
                imageWidth,
                imageHeight
            }
        },
        {
            "BMP",
            "images/image-test-bmp.bmp",
            SDL_Rect{
                firstImageX + 2 * (imageWidth + imageGap),
                imageY,
                imageWidth,
                imageHeight
            }
        },
        {
            "QOI",
            "images/image-test-qoi.qoi",
            SDL_Rect{
                firstImageX + 3 * (imageWidth + imageGap),
                imageY,
                imageWidth,
                imageHeight
            }
        }
    }};

    std::array<TextTexture, 4> imageLabelTextures{};

    const SDL_Rect headingArea{
        24,
        12,
        kLogicalWidth - 48,
        42
    };

    const SDL_Rect statusArea{
        24,
        142,
        kLogicalWidth - 48,
        48
    };

    const SDL_Rect inputArea{
        24,
        kLogicalHeight - 60,
        kLogicalWidth - 48,
        42
    };

    SDL_Rect textInputPosition = inputArea;
    SDL_SetTextInputRect(&textInputPosition);

    const std::string headingText =
        "SDL2_net + IMAGE + TTF + MIXER  "
        "\xEF\x80\xBE";

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

    const std::size_t loadedImageCount =
        load_all_images(renderer, images);

    std::string statusText;

    if (!loopbackResult.success) {
        statusText =
            "NET FAILURE: " +
            loopbackResult.message;
    } else if (loadedImageCount != images.size()) {
        statusText =
            "IMAGE FAILURE: loaded " +
            std::to_string(loadedImageCount) +
            " of 4 formats";
    } else if (!mixerOpen ||
               wavChunk == nullptr ||
               oggMusic == nullptr ||
               mp3Music == nullptr) {
        statusText =
            "IMAGES OK; one or more audio tests unavailable";
    } else {
        statusText =
            "NET LOOPBACK OK; PNG JPEG BMP QOI; audio active";
    }

    std::string textBuffer;
    bool interfaceTexturesDirty = true;
    bool inputTextureDirty = true;
    bool statusTextureDirty = true;
    bool imageTexturesDirty = false;

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
                        "WAV OK: channel sound"
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
                        "OGG OK: music decoder"
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
                        "MP3 OK: music decoder"
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
                    imageTexturesDirty = true;
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

        if (imageTexturesDirty) {
            const std::size_t reloadedCount =
                load_all_images(renderer, images);

            if (reloadedCount == images.size()) {
                set_status(
                    "Renderer reset: all image textures recreated"
                );
            } else {
                set_status(
                    "Renderer reset: only " +
                    std::to_string(reloadedCount) +
                    " of 4 images recreated"
                );
            }

            imageTexturesDirty = false;
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

            for (std::size_t index = 0;
                 index < images.size();
                 ++index) {
                create_text_texture(
                    renderer,
                    font,
                    images[index].label,
                    lightText,
                    imageLabelTextures[index]
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

        for (std::size_t index = 0;
             index < images.size();
             ++index) {
            ImageTest& image = images[index];

            const SDL_Rect labelArea{
                image.destination.x,
                image.destination.y - 34,
                image.destination.w,
                28
            };

            SDL_SetRenderDrawColor(
                renderer,
                image.loaded ? 46 : 150,
                image.loaded ? 56 : 40,
                image.loaded ? 70 : 40,
                255
            );
            SDL_RenderFillRect(renderer, &labelArea);

            draw_text_centered(
                renderer,
                imageLabelTextures[index],
                labelArea
            );

            if (image.texture != nullptr) {
                SDL_RenderCopy(
                    renderer,
                    image.texture,
                    nullptr,
                    &image.destination
                );
            } else {
                SDL_SetRenderDrawColor(
                    renderer,
                    180,
                    40,
                    40,
                    255
                );
                SDL_RenderFillRect(
                    renderer,
                    &image.destination
                );
            }

            SDL_SetRenderDrawColor(
                renderer,
                image.loaded ? 80 : 240,
                image.loaded ? 220 : 70,
                image.loaded ? 140 : 70,
                255
            );
            SDL_RenderDrawRect(
                renderer,
                &image.destination
            );
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

    if (netInitialized) {
        SDLNet_Quit();
    }

    for (ImageTest& image : images) {
        destroy_image(image);
    }

    IMG_Quit();

    destroy_text_texture(inputTexture);
    destroy_text_texture(statusTexture);
    destroy_text_texture(headingTexture);

    for (TextTexture& labelTexture : imageLabelTextures) {
        destroy_text_texture(labelTexture);
    }

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
