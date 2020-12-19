#include "RetroEngine.hpp"

RetroEngine Engine = RetroEngine();

bool processEvents()
{
#if RETRO_USING_SDL
    while (SDL_PollEvent(&Engine.sdlEvents)) {
        switch (Engine.sdlEvents.window.event) {
            case SDL_WINDOWEVENT_MAXIMIZED: {
                SDL_RestoreWindow(Engine.window);
                SDL_SetWindowFullscreen(Engine.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                Engine.isFullScreen = true;
                break;
            }
            case SDL_WINDOWEVENT_CLOSE: return false;
        }

        // Main Events
        switch (Engine.sdlEvents.type) {
            case SDL_CONTROLLERDEVICEADDED: controllerInit(SDL_NumJoysticks() - 1); break;
            case SDL_CONTROLLERDEVICEREMOVED: controllerClose(SDL_NumJoysticks() - 1); break;
            case SDL_WINDOWEVENT_CLOSE:
                if (Engine.window) {
                    SDL_DestroyWindow(Engine.window);
                    Engine.window = NULL;
                }
                return false;
            case SDL_APP_WILLENTERBACKGROUND: /*Engine.Callback(CALLBACK_ENTERBG);*/ break;
            case SDL_APP_WILLENTERFOREGROUND: /*Engine.Callback(CALLBACK_ENTERFG);*/ break;
            case SDL_APP_TERMINATING:  break;
            case SDL_MOUSEMOTION:
                if (SDL_GetNumTouchFingers(SDL_GetTouchDevice(1)) <= 0) { // Touch always takes priority over mouse
                    SDL_GetMouseState(&touchX[0], &touchY[0]);
                    touchX[0] /= Engine.windowScale;
                    touchY[0] /= Engine.windowScale;
                    touches = 1;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (SDL_GetNumTouchFingers(SDL_GetTouchDevice(1)) <= 0) { // Touch always takes priority over mouse
                    switch (Engine.sdlEvents.button.button) {
                        case SDL_BUTTON_LEFT: touchDown[0] = 1; break;
                    }
                    touches = 1;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (SDL_GetNumTouchFingers(SDL_GetTouchDevice(1)) <= 0) { // Touch always takes priority over mouse
                    switch (Engine.sdlEvents.button.button) {
                        case SDL_BUTTON_LEFT: touchDown[0] = 0; break;
                    }
                    touches = 1;
                }
                break;
            case SDL_FINGERMOTION:
                touches = SDL_GetNumTouchFingers(SDL_GetTouchDevice(1));
                for (int i = 0; i < touches; i++) {
                    touchDown[i]       = true;
                    SDL_Finger *finger = SDL_GetTouchFinger(SDL_GetTouchDevice(1), i);
                    touchX[i]          = (finger->x * SCREEN_XSIZE * Engine.windowScale) / Engine.windowScale;

                    touchY[i] = (finger->y * SCREEN_YSIZE * Engine.windowScale) / Engine.windowScale;
                }
                break;
            case SDL_FINGERDOWN:
                touches = SDL_GetNumTouchFingers(SDL_GetTouchDevice(1));
                for (int i = 0; i < touches; i++) {
                    touchDown[i]       = true;
                    SDL_Finger *finger = SDL_GetTouchFinger(SDL_GetTouchDevice(1), i);
                    touchX[i]          = (finger->x * SCREEN_XSIZE * Engine.windowScale) / Engine.windowScale;

                    touchY[i] = (finger->y * SCREEN_YSIZE * Engine.windowScale) / Engine.windowScale;
                }
                break;
            case SDL_KEYDOWN:
                switch (Engine.sdlEvents.key.keysym.sym) {
                    default: break;
                    case SDLK_ESCAPE:
                        if (Engine.devMenu)
                            Engine.gameMode = ENGINE_INITDEVMENU;
                        break;
                    case SDLK_F4:
                        Engine.isFullScreen ^= 1;
                        if (Engine.isFullScreen) {
                            SDL_RestoreWindow(Engine.window);
                            SDL_SetWindowFullscreen(Engine.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        }
                        else {
                            SDL_SetWindowFullscreen(Engine.window, 0);
                            SDL_SetWindowSize(Engine.window, SCREEN_XSIZE * Engine.windowScale, SCREEN_YSIZE * Engine.windowScale);
                            SDL_RestoreWindow(Engine.window);
                        }
                        break;
#if RETRO_PLATFORM == RETRO_OSX
                    case SDLK_TAB: Engine.gameSpeed = Engine.fastForwardSpeed; break;
                    case SDLK_F6:
                        if (Engine.masterPaused)
                            Engine.frameStep = true;
                        break;
                    case SDLK_F7: Engine.masterPaused ^= 1; break;
#else
                    case SDLK_BACKSPACE: Engine.gameSpeed = Engine.fastForwardSpeed; break;
                    case SDLK_F11:
                        if (Engine.masterPaused)
                            Engine.frameStep = true;
                        break;
                    case SDLK_F12: Engine.masterPaused ^= 1; break;
#endif
                }
                break;
            case SDL_KEYUP:
                switch (Engine.sdlEvents.key.keysym.sym) {
                    default:
                        break;
#if RETRO_PLATFORM == RETRO_OSX
                    case SDLK_TAB: Engine.gameSpeed = 1; break;
#else
                    case SDLK_BACKSPACE: Engine.gameSpeed = 1; break;
#endif
                }
                break;
            case SDL_QUIT: return false;
        }
    }
#endif
    return true;
}

void RetroEngine::Init()
{
    CalculateTrigAngles();
    GenerateBlendLookupTable();

    CheckRSDKFile("data.rsdk");
    InitUserdata();
    InitNativeObjectSystem();

    gameMode = ENGINE_MAINGAME;
    running  = false;
    LoadGameConfig("Data/Game/GameConfig.bin");
    if (InitRenderDevice()) {
        if (InitAudioPlayback()) {
            InitFirstStage();
            ClearScriptData();
            initialised = true;
            running     = true;
        }
    }
}

void RetroEngine::Run()
{
    uint frameStart, frameEnd = SDL_GetTicks();
    float frameDelta = 0.0f;

    while (running) {
        frameStart = SDL_GetTicks();
        frameDelta = frameStart - frameEnd;

        if (frameDelta > 1000.0f / (float)refreshRate) {
            frameEnd = frameStart;

            running = processEvents();

            for (int s = 0; s < gameSpeed; ++s) {
                ProcessInput();

                if (!masterPaused || frameStep) {
                    ProcessNativeObjects();

                    RenderRenderDevice();

                    frameStep = false;
                }
            }
        }
    }

    ReleaseAudioDevice();
    ReleaseRenderDevice();
    writeSettings();

#if RETRO_USING_SDL
    SDL_Quit();
#endif
}

void RetroEngine::LoadGameConfig(const char *filePath)
{
    FileInfo info;
    int fileBuffer  = 0;
    int fileBuffer2 = 0;
    char strBuffer[0x40];

    if (LoadFile(filePath, &info)) {
        FileRead(&fileBuffer, 1);
        FileRead(gameWindowText, fileBuffer);
        gameWindowText[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(gameDescriptionText, fileBuffer);
        gameDescriptionText[fileBuffer] = 0;

        byte buf[3];
        for (int c = 0; c < 0x60; ++c) {
            FileRead(buf, 3);
            SetPaletteEntry(-1, c, buf[0], buf[1], buf[2]);
        }

        // Read Obect Names
        int objectCount = 0;
        FileRead(&objectCount, 1);
        for (int o = 0; o < objectCount; ++o) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
        }

        // Read Script Paths
        for (int s = 0; s < objectCount; ++s) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
        }

        int varCount = 0;
        FileRead(&varCount, 1);
        globalVariablesCount = varCount;
        for (int v = 0; v < varCount; ++v) {
            // Read Variable Name
            FileRead(&fileBuffer, 1);
            FileRead(&globalVariableNames[v], fileBuffer);
            globalVariableNames[v][fileBuffer] = 0;

            // Read Variable Value
            FileRead(&fileBuffer2, 1);
            globalVariables[v] = fileBuffer2 << 0;
            FileRead(&fileBuffer2, 1);
            globalVariables[v] += fileBuffer2 << 8;
            FileRead(&fileBuffer2, 1);
            globalVariables[v] += fileBuffer2 << 16;
            FileRead(&fileBuffer2, 1);
            globalVariables[v] += fileBuffer2 << 24;
        }

        // Read SFX
        int globalSFXCount = 0;
        FileRead(&globalSFXCount, 1);
        for (int s = 0; s < globalSFXCount; ++s) { // SFX Names
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;
        }
        for (int s = 0; s < globalSFXCount; ++s) { // SFX Paths
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;
        }

        // Read Player Names
        int playerCount = 0;
        FileRead(&playerCount, 1);
        for (int p = 0; p < playerCount; ++p) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
        }

        for (int c = 0; c < 4; ++c) {
            // Special Stages are stored as cat 2 in file, but cat 3 in game :(
            int cat = c;
            if (c == 2)
                cat = 3;
            else if (c == 3)
                cat = 2;
            stageListCount[cat] = 0;
            FileRead(&stageListCount[cat], 1);
            for (int s = 0; s < stageListCount[cat]; ++s) {

                // Read Stage Folder
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].folder, fileBuffer);
                stageList[cat][s].folder[fileBuffer] = 0;

                // Read Stage ID
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].id, fileBuffer);
                stageList[cat][s].id[fileBuffer] = 0;

                // Read Stage Name
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].name, fileBuffer);
                stageList[cat][s].name[fileBuffer] = 0;

                // Read Stage Mode
                FileRead(&fileBuffer, 1);
                stageList[cat][s].highlighted = fileBuffer;
            }
        }

        CloseFile();
    }

    
    //These need to be set every time its reloaded
    nativeFunctionCount = 0;
    AddNativeFunction("SetAchievement", SetAchievement);
    AddNativeFunction("SetLeaderboard", SetLeaderboard);
    AddNativeFunction("Connect2PVS", Connect2PVS);
    AddNativeFunction("Disconnect2PVS", Disconnect2PVS);
    AddNativeFunction("SendEntity", SendEntity);
    AddNativeFunction("SendValue", SendValue);
    AddNativeFunction("ReceiveEntity", ReceiveEntity);
    AddNativeFunction("ReceiveValue", ReceiveValue);
    AddNativeFunction("TransmitGlobal", TransmitGlobal);
    AddNativeFunction("ShowPromoPopup", ShowPromoPopup);
}

void RetroEngine::Callback(int callbackID)
{
    switch (callbackID) {
        default:
#if RSDK_DEBUG
            printLog("Callback: Unknown (%d)", callbackID);
#endif
            break;
    }
}
