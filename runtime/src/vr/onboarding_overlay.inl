// Included inside settings_overlay's private namespace.
mkw::vr::TutorialFlow g_tutorial;
std::string g_onboardingError;

namespace {
using TutorialModel=std::vector<AuroraVRControllerVertex>;

AuroraVRControllerVertex TutorialModelVertex(float x,float y,float z,float r,float g,float b) {
    return {{x,y,z},{r,g,b}};
}

void AddTutorialBox(TutorialModel& out,float cx,float cy,float cz,float sx,float sy,float sz,
                    float r,float g,float b) {
    const float x0=cx-sx*.5f,x1=cx+sx*.5f;
    const float y0=cy-sy*.5f,y1=cy+sy*.5f;
    const float z0=cz-sz*.5f,z1=cz+sz*.5f;
    const AuroraVRControllerVertex v[8]={
        TutorialModelVertex(x0,y0,z0,r,g,b),TutorialModelVertex(x1,y0,z0,r,g,b),
        TutorialModelVertex(x1,y1,z0,r,g,b),TutorialModelVertex(x0,y1,z0,r,g,b),
        TutorialModelVertex(x0,y0,z1,r,g,b),TutorialModelVertex(x1,y0,z1,r,g,b),
        TutorialModelVertex(x1,y1,z1,r,g,b),TutorialModelVertex(x0,y1,z1,r,g,b)};
    constexpr uint8_t tri[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
    for(uint8_t i:tri) out.push_back(v[i]);
}

void AddTutorialRing(TutorialModel& out,float cy,float cz,float radius,float thickness,float r,float g,float b) {
    constexpr int segments=20;
    for(int i=0;i<segments;++i) {
        const float a=float(i)*6.28318530718f/segments;
        const float n=float(i+1)*6.28318530718f/segments;
        const float ca=std::cos(a),sa=std::sin(a),cn=std::cos(n),sn=std::sin(n);
        const float ro=radius+thickness*.5f,ri=radius-thickness*.5f;
        const auto a0=TutorialModelVertex(ca*ro,cy+sa*ro,cz,r,g,b);
        const auto a1=TutorialModelVertex(ca*ri,cy+sa*ri,cz,r,g,b);
        const auto b0=TutorialModelVertex(cn*ro,cy+sn*ro,cz,r,g,b);
        const auto b1=TutorialModelVertex(cn*ri,cy+sn*ri,cz,r,g,b);
        out.insert(out.end(),{a0,b0,b1,a0,b1,a1});
    }
}

TutorialModel MakeQuestTutorialController(bool right) {
    TutorialModel out;
    const float tint=.08f;
    AddTutorialBox(out,0,-.045f,.018f,.035f,.11f,.037f,tint,tint+.05f,tint+.08f);
    AddTutorialBox(out,0,.018f,-.005f,.07f,.055f,.045f,.18f,.22f,.28f);
    AddTutorialRing(out,.055f,-.012f,.048f,.010f,.12f,.16f,.21f);
    AddTutorialBox(out,right?.020f:-.020f,.035f,-.031f,.018f,.012f,.010f,.85f,.88f,.92f);
    AddTutorialBox(out,right?-.014f:.014f,.018f,-.034f,.022f,.018f,.010f,.32f,.36f,.42f);
    return out;
}

bool PublishFallbackTutorialControllers() {
    static const auto left=MakeQuestTutorialController(false);
    static const auto right=MakeQuestTutorialController(true);
    aurora_set_vr_controller_model(0,left.data(),static_cast<uint32_t>(left.size()));
    aurora_set_vr_controller_model(1,right.data(),static_cast<uint32_t>(right.size()));
    return true;
}

#if defined(_WIN32)
HMODULE LoadOpenVrLibraryForTutorial() {
    if(auto* loaded=GetModuleHandleW(L"openvr_api.dll")) return loaded;
    if(auto* direct=LoadLibraryW(L"openvr_api.dll")) return direct;
    wchar_t steamPath[1024]{};DWORD size=sizeof(steamPath);DWORD type=0;
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,&type,steamPath,&size)==ERROR_SUCCESS) {
        std::wstring path=steamPath;
        std::replace(path.begin(),path.end(),L'/',L'\\');
        path+=L"\\steamapps\\common\\SteamVR\\bin\\win64\\openvr_api.dll";
        if(auto* fromSteam=LoadLibraryW(path.c_str())) return fromSteam;
    }
    return nullptr;
}

#include "steam_controller_models.inl"

bool PublishSteamVrTutorialControllers() {
    using InitFn=uint32_t (*)(EVRInitError*,EVRApplicationType);
    using GetInterfaceFn=intptr_t (*)(const char*,EVRInitError*);
    HMODULE library=LoadOpenVrLibraryForTutorial();
    if(!library) return false;
    const auto init=reinterpret_cast<InitFn>(GetProcAddress(library,"VR_InitInternal"));
    const auto getInterface=reinterpret_cast<GetInterfaceFn>(GetProcAddress(library,"VR_GetGenericInterface"));
    if(!init || !getInterface) return false;
    EVRInitError error=EVRInitError_VRInitError_None;
    // Utility clients cannot query tracked devices. A background client can
    // read controller models without taking the OpenXR scene application's focus.
    // Keep this shared OpenVR client alive: shutting it down here can invalidate
    // interfaces still used by SDL or the SteamVR OpenXR runtime.
    static bool initialized=false;
    if(!initialized) {
        init(&error,EVRApplicationType_VRApplication_Background);
        initialized=error==EVRInitError_VRInitError_None;
    }
    if(error!=EVRInitError_VRInitError_None) {
        std::fprintf(stderr,"[vr-tutorial] OpenVR background initialization failed: %d; using fallback models\n",int(error));
        return false;
    }
    const std::string systemName=std::string("FnTable:")+IVRSystem_Version;
    const std::string renderName=std::string("FnTable:")+IVRRenderModels_Version;
    auto* system=reinterpret_cast<VR_IVRSystem_FnTable*>(getInterface(systemName.c_str(),&error));
    if(!system || error!=EVRInitError_VRInitError_None) return false;
    auto* render=reinterpret_cast<VR_IVRRenderModels_FnTable*>(getInterface(renderName.c_str(),&error));
    if(!render || error!=EVRInitError_VRInitError_None) return false;
    const std::string inputName=std::string("FnTable:")+IVRInput_Version;
    auto* input=reinterpret_cast<VR_IVRInput_FnTable*>(getInterface(inputName.c_str(),&error));
    return UpdateSteamControllerModels(system,render,error==EVRInitError_VRInitError_None?input:nullptr);
}
#endif

const char* ControllerKey(int hand,int row) {
#if defined(_WIN32)
    return g_controllerKeys[hand][row].c_str();
#else
    static const char* keys[2][5]={{"STICK","X","Y","TRIGGER","GRIP"},{"STICK CLICK","A","B","TRIGGER","GRIP"}};
    return keys[hand][row];
#endif
}

void EnsureTutorialControllerModels(bool steamvr) {
    static bool fallbackPublished=false;
    static double lastUpdate=-1;
    if(!fallbackPublished) fallbackPublished=PublishFallbackTutorialControllers();
#if defined(_WIN32)
    if(steamvr && lastUpdate!=ImGui::GetTime()) {
        lastUpdate=ImGui::GetTime();
        PublishSteamVrTutorialControllers();
    }
#else
    (void)steamvr;
#endif
}
}

void VrPointer(const mkw::vr::QuestInput& input) {
    EnsureTutorialControllerModels(input.steamvr);
    auto& io=ImGui::GetIO();
    const auto* viewport=ImGui::GetMainViewport();
    const float width=RuntimeConfigFile::VrHudWidthMeters(2.4f);
    // Match the desktop UI snapshot projected onto the anchored panel.
    const float height=width*viewport->Size.y/std::max(1.0f,viewport->Size.x);
    float u=0,v=0,leftU=0,leftV=0;
    const bool rightHit=input.active && mkw::vr::ProjectUiRay(input.ui_pointer,width,height,
        RuntimeConfigFile::VrHudDistanceMeters(2),u,v);
    const bool leftHit=input.active && mkw::vr::ProjectUiRay(input.ui_left_pointer,width,height,
        RuntimeConfigFile::VrHudDistanceMeters(2),leftU,leftV);
    static bool wasDown=false;
    static bool leftSelected=false;
    if(!wasDown) {
        if(leftHit && input.reverse) leftSelected=true;
        else if(rightHit && input.accelerate>.55f) leftSelected=false;
        else if(leftSelected?!leftHit:!rightHit) leftSelected=leftHit;
    }
    const bool hit=leftSelected?leftHit:rightHit;
    if(leftSelected) {u=leftU;v=leftV;}
    const bool down=hit && (leftSelected?input.reverse:input.accelerate>.55f);
    if(hit) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        const ImVec2 point{viewport->Pos.x+u*viewport->Size.x,viewport->Pos.y+v*viewport->Size.y};
        aurora_set_vr_ui_pointer(point.x,point.y,true,down);
        auto* draw=ImGui::GetForegroundDrawList();
        draw->AddCircle(point,9,IM_COL32(40,235,205,255),20,2);
        draw->AddCircleFilled(point,3,IM_COL32(255,255,255,255));
    }
    if(!hit) aurora_set_vr_ui_pointer(0,0,false,false);
    wasDown=down;
}

bool BeginIntroduction(const char* title) {
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowBgAlpha(1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(30,24));
    const bool open=ImGui::Begin(title,nullptr,ImGuiWindowFlags_NoDecoration|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.15f);
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    return open;
}
void EndIntroduction() { ImGui::End();ImGui::PopStyleVar(); }

void DrawWelcomePanel() {
    const auto input=mkw::vr::ReadQuestInputSnapshot();
    VrPointer(input);
    mkw::vr::MkwVRPolicySetSettingsVisible(true);
    if(BeginIntroduction("Welcome to Mario Kart Wii VR")) {
        ImGui::TextWrapped("Choose the camera used at the start of every race. Point with your right controller and pull its trigger to select. You can also use the mouse.");
        ImGui::Spacing();
        static int choice=-1;
        if(choice<0) choice=RuntimeConfigFile::Get().vrDefaultCamera;
        const char* titles[]{"Original / third person","First person","Diorama"};
        const char* descriptions[]{
            "Follow your kart with the original game camera. See the driver and steer with the left stick.",
            "Sit in the driver's seat. Grab the real wheel or handlebars with the grips and steer with your hands. A hops and drifts.",
            "Watch the race from a distant elevated viewpoint, like a miniature circuit. Use the same controls as third person."};
        for(int mode=0;mode<3;++mode) {
            ImGui::PushID(mode);
            if(ImGui::RadioButton(titles[mode],choice==mode)) choice=mode;
            ImGui::Indent(24);ImGui::TextWrapped("%s",descriptions[mode]);ImGui::Unindent(24);
            ImGui::Spacing();ImGui::PopID();
        }
        ImGui::TextWrapped("Right stick click cycles cameras at any time. In your first race, a short guide will explain the controls. A second guide appears when you first use the other driving mode.");
        if(!g_onboardingError.empty()) ImGui::TextWrapped("%s",g_onboardingError.c_str());
        ImGui::Spacing();
        if(ImGui::Button("Save camera and start game",ImVec2(-1,52))) {
            const bool saved=RuntimeConfigFile::WriteSetting("vr","default_camera",std::to_string(choice)) &&
                RuntimeConfigFile::WriteSetting("vr","welcome_complete","true");
            if(saved) {
                RuntimeConfigFile::Mutable().vrDefaultCamera=choice;
                RuntimeConfigFile::Mutable().vrWelcomeComplete=true;
                mkw::vr::MkwVRSetCameraMode(static_cast<mkw::vr::CameraMode>(choice));
                mkw::vr::MkwVRPolicySetSettingsVisible(false);g_onboardingError.clear();
            } else g_onboardingError="Could not save your choice. Check the installation folder permissions and try again.";
        }
    }
    EndIntroduction();
}

bool MarioKartPaused() {
    // Read the game's own predicate (PAL RaceScene::isPaused), using a saved
    // register context. It covers RacePauseMgr and the current section's pause.
    auto* cpu=TryGetCpuContext();
    if(!cpu) return false;
    CpuContext probe=*cpu;
    InvokeIndirectCpu(0x80554E14u,&probe);
    return probe.gpr[3]!=0;
}

void DrawControllerGuide(const mkw::vr::QuestInput& input,bool cockpit) {
    EnsureTutorialControllerModels(input.steamvr);
    AuroraVRUiGuide guide{};guide.active=true;
    const auto* viewport=ImGui::GetMainViewport();
    const bool swap=RuntimeConfigFile::Get().vrSwapItemTrick;
    const bool swapDrift=RuntimeConfigFile::Get().vrSwapCockpitDriftBrake;
    const auto area=ImGui::GetContentRegionAvail();
    const float column=area.x*.5f;
    const auto top=ImGui::GetCursorScreenPos();
    guide.headerEnd=(top.y-viewport->Pos.y)/viewport->Size.y;
    const float graphicHeight=std::min(360.0f,std::max(220.0f,area.y-130));
    auto* draw=ImGui::GetWindowDrawList();
    for(int hand=0;hand<2;++hand) {
        const float left=top.x+column*hand;
        ImVec2 center{left+column*.24f,top.y+graphicHeight*.45f};
        // The model outline and button callouts follow tracked hand motion in
        // a bounded teaching panel, keeping every label within reading reach.
        const auto& pose=input.ui_hands[hand];
        if(pose.valid) {
            const auto anchor=mkw::vr::ControllerCalloutAnchor(pose,hand?1.0f:-1.0f);
            center.x+=std::clamp(anchor[0]*55,-24.0f,24.0f);
            center.y-=std::clamp(anchor[1]*55,-24.0f,24.0f);
        }
        const ImU32 accent=hand?IM_COL32(245,190,80,255):IM_COL32(40,225,210,255);
        draw->AddText({left+12,top.y},accent,hand?"RIGHT CONTROLLER":"LEFT CONTROLLER");
        draw->AddCircleFilled(center,42,IM_COL32(54,65,82,255),32);
        draw->AddRectFilled({center.x-23,center.y+16},{center.x+22,center.y+117},IM_COL32(54,65,82,255),18);
        const auto label=[&](ImVec2 button,float row,const char* key,const char* action,bool pressed) {
            const ImVec2 text{left+column*.43f,top.y+24+row*38};
            auto* uv=guide.labels[hand*5+int(row)];
            uv[0]=(text.x-viewport->Pos.x)/viewport->Size.x;
            uv[1]=(text.y-viewport->Pos.y)/viewport->Size.y;
            uv[2]=(left+column-4-viewport->Pos.x)/viewport->Size.x;
            uv[3]=(text.y+35-viewport->Pos.y)/viewport->Size.y;
            draw->AddCircleFilled(button,pressed?9:6,pressed?IM_COL32(255,255,255,255):accent);
            draw->AddLine(button,{text.x-6,text.y+7},accent,1.5f);
            draw->AddText(text,accent,key);
            draw->AddText({text.x,text.y+18},IM_COL32(240,240,240,255),action);
        };
        if(!hand) {
            label({center.x-15,center.y-16},0,ControllerKey(hand,0),cockpit?"Steer / aim item":"Steer",std::abs(input.steering_x)>.3f);
            label({center.x+13,center.y+10},1,ControllerKey(hand,1),swap?"Use item":"Trick / wheelie",input.trick);
            label({center.x+21,center.y-12},2,ControllerKey(hand,2),swap?"Trick / wheelie":"Use item",input.item>.5f);
            label({center.x,center.y-38},3,"TRIGGER","Brake / reverse",input.reverse);
            label({center.x-23,center.y+57},4,"GRIP",cockpit?"Hold wheel":"Not used",input.ui_grips[0]>.5f);
        } else {
            label({center.x-15,center.y-16},0,ControllerKey(hand,0),"Change camera",false);
            label({center.x+13,center.y+10},1,"A",cockpit?(swapDrift?"Brake":"Hop / drift"):"Confirm / accelerate",input.confirm);
            label({center.x+21,center.y-12},2,"B",cockpit&&swapDrift?"Hop / drift":"Brake / back",input.brake);
            label({center.x,center.y-38},3,"TRIGGER","Accelerate",input.accelerate>.5f);
            label({center.x-23,center.y+57},4,"GRIP",cockpit?"Hold wheel":"Hop / drift",input.ui_grips[1]>.5f);
        }
    }
    ImGui::Dummy(ImVec2(area.x,graphicHeight));
    guide.footerStart=(ImGui::GetCursorScreenPos().y-viewport->Pos.y)/viewport->Size.y;
    aurora_set_vr_ui_guide(&guide);
    ImGui::TextWrapped("Right stick directions: directional tricks. Left %s + %s: VR options.",ControllerKey(0,1),ControllerKey(0,2));
    if(input.steamvr) ImGui::TextWrapped("Hold left %s 0.65 s: Mario Kart pause. Menu: SteamVR dashboard.",ControllerKey(0,1));
    else ImGui::TextWrapped("Menu: Mario Kart pause.");
    if(cockpit) ImGui::TextWrapped("Hold either grip near the wheel. Release both grips to steer with the stick. The wheel keeps its centre beyond full lock.");
    if(!input.ui_hands[0].valid || !input.ui_hands[1].valid) ImGui::TextDisabled("Raise your controllers to see live button highlights.");
}

void DrawRaceTutorial() {
    aurora_set_vr_ui_guide(nullptr);
    if(!g_vrEnabled || !RuntimeConfigFile::Get().vrWelcomeComplete) return;
    if(RuntimeConfigFile::Get().vrTutorialCompleted==3 && g_tutorial.stage==mkw::vr::TutorialFlow::Stage::Idle) return;
    const auto policy=mkw::vr::MkwVRPolicyGetSnapshot();
    const bool race=!mkw::vr::MkwVRRaceIntroActive() && policy.session_active && policy.scene.mode==mkw::vr::VRSceneMode::Race &&
        policy.scene.local_player_count==1 && policy.camera.valid;
    const bool cockpit=mkw::vr::MkwVRGetCameraMode()==mkw::vr::CameraMode::FirstPerson;
    const auto input=mkw::vr::ReadQuestInputSnapshot();
    const bool paused=race && MarioKartPaused();
    const auto previous=g_tutorial.stage;
    if((!race || (!g_vrSettingsVisible && input.active)) && g_tutorial.Update(race,cockpit,paused,
        RuntimeConfigFile::Get().vrTutorialCompleted,ImGui::GetTime())) mkw::vr::RequestQuestPausePulse();
    if(!race && previous==mkw::vr::TutorialFlow::Stage::Showing) mkw::vr::MkwVRPolicySetSettingsVisible(g_vrSettingsVisible);
    if(g_tutorial.stage!=mkw::vr::TutorialFlow::Stage::Showing) return;
    mkw::vr::MkwVRPolicySetSettingsVisible(true);
    PADBlockInput(true);
    VrPointer(input);
    if(BeginIntroduction(g_tutorial.bit==2?"First-person driving controls":"Third-person and diorama controls")) {
        ImGui::TextWrapped("Mario Kart is paused. Point at Continue with your right controller and pull its trigger when you are ready.");
        DrawControllerGuide(input,g_tutorial.bit==2);
        if(!g_onboardingError.empty()) ImGui::TextWrapped("%s",g_onboardingError.c_str());
        if(ImGui::Button("Continue racing",ImVec2(-1,48))) {
            const unsigned completed=RuntimeConfigFile::Get().vrTutorialCompleted|g_tutorial.bit;
            if(RuntimeConfigFile::WriteSetting("vr","tutorial_completed",std::to_string(completed))) {
                RuntimeConfigFile::Mutable().vrTutorialCompleted=completed;
                g_tutorial={};g_onboardingError.clear();
                mkw::vr::MkwVRPolicySetSettingsVisible(g_vrSettingsVisible);
                if(paused) mkw::vr::RequestQuestPausePulse();
            } else g_onboardingError="Could not save tutorial progress. Check folder permissions and try again.";
        }
    }
    EndIntroduction();
}
