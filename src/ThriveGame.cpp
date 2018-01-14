// ------------------------------------ //
#include "ThriveGame.h"

#include "thrive_net_handler.h"
#include "thrive_version.h"
#include "thrive_world_factory.h"

#include "main_menu_keypresses.h"

#include "microbe_stage/simulation_parameters.h"
#include "microbe_stage/biome_controller.h"

#include "general/locked_map.h"

#include "engine/player_data.h"

#include "generated/cell_stage_world.h"

#include "Handlers/ObjectLoader.h"

#include "Networking/NetworkHandler.h"
#include "Rendering/GraphicalInputEntity.h"

#include "GUI/AlphaHitCache.h"

#include "Script/Bindings/BindHelpers.h"
#include "Script/Bindings/StandardWorldBindHelper.h"

#include "CEGUI/SchemeManager.h"

#include <Addons/GameModule.h>

using namespace thrive;

// ------------------------------------ //
class ThriveGame::Implementation{
public:
    Implementation(
        ThriveGame& game
    ) : m_game(game),
        m_playerData("player"),
        m_menuKeyPresses(std::make_shared<MainMenuKeyPressListener>())
    {
    }

    ThriveGame& m_game;
    
    PlayerData m_playerData;

    // This contains all the microbe_stage AngelScript code
    Leviathan::GameModule::pointer m_MicrobeScripts;

    std::shared_ptr<MainMenuKeyPressListener> m_menuKeyPresses;
};

// ------------------------------------ //
ThriveGame::ThriveGame(){
    m_impl = std::make_unique<Implementation>(*this);
    StaticGame = this;
}

ThriveGame::~ThriveGame(){

    StaticGame = nullptr;
}

std::string ThriveGame::GenerateWindowTitle(){

    return "Thrive " GAME_VERSIONS;
}

ThriveGame* ThriveGame::Get(){

    return StaticGame;
}

ThriveGame* ThriveGame::instance(){

    return StaticGame;
}

ThriveGame* ThriveGame::StaticGame = nullptr;

Leviathan::NetworkInterface* ThriveGame::_GetApplicationPacketHandler(){

    if(!Network)
        Network = std::make_unique<ThriveNetHandler>();
    return Network.get();
}

void ThriveGame::_ShutdownApplicationPacketHandler(){

    Network.reset();
}
// ------------------------------------ //

void ThriveGame::startNewGame(){

    // To work with instant start, we need to invoke this if we have no cell stage world
    if(!m_postLoadRan){

        Engine::Get()->Invoke([=](){
                startNewGame();
            });
        return;
    }

    Leviathan::Engine* engine = Engine::GetEngine();

    LOG_INFO("New game started");

    Leviathan::GraphicalInputEntity* window1 = engine->GetWindowEntity();

    // Create world if not already created //
    if(!m_cellStage){
   
        LOG_INFO("ThriveGame: startNewGame: Creating new cellstage world");
        m_cellStage = std::dynamic_pointer_cast<CellStageWorld>(engine->CreateWorld(
                window1));
    }
    
    LEVIATHAN_ASSERT(m_cellStage, "Cell stage world creation failed");
    
    window1->LinkObjects(m_cellStage);

    // Clear world //
    m_cellStage->ClearEntities();

    // TODO: unfreeze, if was in the background

    // Main camera that will be attached to the player
    m_cellCamera = Leviathan::ObjectLoader::LoadCamera(*m_cellStage, Float3(0, 15, 0),
        Ogre::Quaternion(Ogre::Degree(-90), Ogre::Vector3::UNIT_X)
    );

    // TODO: attach a ligth to the camera
    // -- Light
    //     local light = OgreLightComponent.new()
    //     light:setRange(200)
    //     entity:addComponent(light)

    m_cellStage->SetCamera(m_cellCamera);

	// This is here for testing purposes only.
	SimulationParameters::init();
	BiomeController bc;
	size_t currentBiomeid = bc.getCurrentBiome();
	std::string background = SimulationParameters::biomeRegistry.getTypeData(currentBiomeid).background;

    // Setup compound clouds //
    const auto compoundCount = SimulationParameters::compoundRegistry.getSize();

    for(size_t i = 0; i < compoundCount; ++i){

        const auto& data = SimulationParameters::compoundRegistry.getTypeData(i);

        if(!data.isCloud)
            continue;

        auto cloudId = m_cellStage->CreateEntity();
        m_cellStage->Create_CompoundCloudComponent(cloudId, data.id,
            data.colour.r, data.colour.g, data.colour.b);
    }

    // Set background plane //
	if (true) {
		m_backgroundPlane = Leviathan::ObjectLoader::LoadPlane(*m_cellStage, Float3(0, -50, 0),
			Ogre::Quaternion(Ogre::Degree(90), Ogre::Vector3::UNIT_Z) *
			Ogre::Quaternion(Ogre::Degree(45), Ogre::Vector3::UNIT_Y),
			background, Ogre::Plane(1, 1, 1, 1),
			Float2(200, 200));
	}

    // Spawn player //
    respawnPlayerCell();
   
	// Test model //
    if(false){
        const auto testModel = m_cellStage->CreateEntity();
        m_cellStage->Create_Position(testModel, Float3(0, 0, 0), Ogre::Quaternion(Ogre::Degree(90), Ogre::Vector3::UNIT_X));
        auto& node = m_cellStage->Create_RenderNode(testModel);
        m_cellStage->Create_Model(testModel, node.Node, "nucleus.mesh");
    }
}

void ThriveGame::respawnPlayerCell(){
    LEVIATHAN_ASSERT(m_playerCell == 0, "Player alive in respawnPlayercell");

    m_playerCell = m_cellStage->CreateEntity();

    m_cellStage->Create_RenderNode(m_playerCell);
	auto& processor = m_cellStage->Create_ProcessorComponent(m_playerCell);
	auto& compoundBag = m_cellStage->Create_CompoundBagComponent(m_playerCell);
	m_cellStage->Create_SpeciesComponent(m_playerCell, "PIKACHU");

    m_cellStage->Create_Position(m_playerCell, Float3(0), Float4::IdentityQuaternion());

    MembraneComponent& membrane = m_cellStage->Create_MembraneComponent(m_playerCell);
    for(int x = -3; x <= 3; ++x){
        for(int y = -3; y <= 3; ++y){
            membrane.sendOrganelles(x, y);
        }
    }
}

// ------------------------------------ //
CellStageWorld* ThriveGame::getCellStage(){

    return m_cellStage.get();
}

PlayerData&
ThriveGame::playerData(){
    return m_impl->m_playerData;
}
// ------------------------------------ //
void ThriveGame::onIntroSkipPressed(){

    // Fire an event that the GUI handles //
    Engine::Get()->GetEventHandler()->CallEvent(
        new Leviathan::GenericEvent("MainMenuIntroSkipEvent"));
}

// ------------------------------------ //
void ThriveGame::Tick(int mspassed){

    dummyTestCounter += mspassed;

    float radians = dummyTestCounter / 500.f;

    if(m_playerCell && false){

        Leviathan::Position& pos = m_cellStage->GetComponent_Position(m_playerCell);

        pos.Members._Orientation = Ogre::Quaternion::IDENTITY * Ogre::Quaternion(
            Ogre::Radian(radians), Ogre::Vector3::UNIT_X);

        pos.Marked = true;
    }

	if (m_playerCell && false) {

		Leviathan::Position& pos = m_cellStage->GetComponent_Position(m_cellCamera);

		pos.Members._Position += Leviathan::Float3(mspassed * 1.0 / 1000.0, 0, 0);

		pos.Marked = true;
	}

    if(m_backgroundPlane != 0 && false){

        auto& node = m_cellStage->GetComponent_RenderNode(m_backgroundPlane);
        node.Hidden = false;
        node.Marked = true;

        Leviathan::Position& pos = m_cellStage->GetComponent_Position(m_backgroundPlane);

        pos.Members._Orientation = Ogre::Quaternion::IDENTITY * Ogre::Quaternion(
            Ogre::Radian(radians), Ogre::Vector3::UNIT_Y);

        pos.Marked = true;
    }

    if(m_cellCamera != 0 && false){
    
        Leviathan::Position& pos = m_cellStage->GetComponent_Position(m_cellCamera);

        pos.Members._Orientation = Ogre::Quaternion(
            Ogre::Radian(radians), Ogre::Vector3::UNIT_X);

        pos.Marked = true;
    }
}

void ThriveGame::CustomizeEnginePostLoad(){

    Engine* engine = Engine::Get();

    // Load scripts
    LOG_INFO("ThriveGame: loading main scripts");

    // TODO: should these load failures be fatal errors (process would exit immediately)

    try{
        m_impl->m_MicrobeScripts = Leviathan::GameModule::MakeShared<Leviathan::GameModule>(
            "microbe_stage", "ThriveGame");
    } catch(const Leviathan::Exception &e){

        LOG_ERROR("ThriveGame: microbe_stage module failed to load, exception:");
        e.PrintToLog();
        MarkAsClosing();
        return;        
    }

    if(!m_impl->m_MicrobeScripts->Init()){

        LOG_ERROR("ThriveGame: microbe_stage module init failed");
        MarkAsClosing();
        return;
    }

    LOG_INFO("ThriveGame: script loading succeeded");
    

    // This is fine to set here to avoid putting this behind the next no gui check //
    m_postLoadRan = true;

    // Load GUI documents (but only if graphics are enabled) //
    if(engine->GetNoGui()){
        
        // Skip the graphical objects when not in graphical mode //
        return;
    }

    // Load the thrive gui theme //
    Leviathan::GUI::GuiManager::LoadGUITheme("Thrive.scheme");

    Leviathan::GraphicalInputEntity* window1 = Engine::GetEngine()->GetWindowEntity();

    // Register custom listener for detecting keypresses for skipping the intro video
    window1->GetInputController()->LinkReceiver(m_impl->m_menuKeyPresses);

    Leviathan::GUI::GuiManager* GuiManagerAccess = window1->GetGui();

    // Enable thrive mouse and tooltip style //
    GuiManagerAccess->SetMouseTheme("ThriveGeneric/MouseArrow");
    GuiManagerAccess->SetTooltipType("Thrive/Tooltip");

    Leviathan::GUI::AlphaHitCache* cache = Leviathan::GUI::AlphaHitCache::Get();
    
    // One image from each used alphahit texture should be
    // loaded. Loading all from each set is probably only a tiny bit
    // faster during gameplay so that it is not worth the effort here
    cache->PreLoadImage("ThriveGeneric/MenuNormal");
        
    if(!GuiManagerAccess->LoadGUIFile("./Data/Scripts/gui/thrive_menus.txt")){
        
        LOG_ERROR("Thrive: failed to load the main menu gui, quitting");
        StartRelease();
        return;
    }
}

void ThriveGame::EnginePreShutdown(){

    // Shutdown scripting first to allow it to still do anything it wants //
    m_impl->m_MicrobeScripts->ReleaseScript();
    m_impl->m_MicrobeScripts.reset();
    
    // All resources that need Ogre or the engine to be available when
    // they are destroyed need to be released here
    
    m_cellStage.reset();

    m_impl.reset();
}
// ------------------------------------ //
void ThriveGame::CheckGameConfigurationVariables(Lock &guard, GameConfiguration* configobj){
    
}

void ThriveGame::CheckGameKeyConfigVariables(Lock &guard, KeyConfiguration* keyconfigobj){

}
// ------------------------------------ //
bool registerLockedMap(asIScriptEngine* engine){

    if(engine->RegisterObjectType("LockedMap", 0, asOBJ_REF | asOBJ_NOCOUNT) < 0){
        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectMethod("LockedMap",
            "void addLock(string lockName)",
            asMETHOD(LockedMap, addLock),
            asCALL_THISCALL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectMethod("LockedMap",
            "bool isLocked(string conceptName)",
            asMETHOD(LockedMap, isLocked),
            asCALL_THISCALL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectMethod("LockedMap",
            "void unlock(string conceptName)",
            asMETHOD(LockedMap, unlock),
            asCALL_THISCALL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }

    return true;
}

bool registerPlayerData(asIScriptEngine* engine){

    if(engine->RegisterObjectType("PlayerData", 0, asOBJ_REF | asOBJ_NOCOUNT) < 0){
        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectMethod("PlayerData",
            "LockedMap& lockedMap()",
            asMETHOD(PlayerData, lockedMap),
            asCALL_THISCALL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }
    
    return true;
}

bool ThriveGame::InitLoadCustomScriptTypes(asIScriptEngine* engine){

    if(!registerLockedMap(engine))
        return false;

    if(!registerPlayerData(engine))
        return false;

    if(engine->RegisterObjectType("ThriveGame", 0, asOBJ_REF | asOBJ_NOCOUNT) < 0){
        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterGlobalFunction("ThriveGame@ GetThriveGame()",
            asFUNCTION(ThriveGame::Get), asCALL_CDECL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }

    ANGLESCRIPT_BASE_CLASS_CASTS_NO_REF(LeviathanApplication, "LeviathanApplication",
        ThriveGame, "ThriveGame");

    if(engine->RegisterObjectMethod("ThriveGame",
            "PlayerData& playerData()",
            asMETHOD(ThriveGame, playerData),
            asCALL_THISCALL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }

    // if(engine->RegisterObjectMethod("ThriveGame",
    //         "SoundPlayer@ getGuiSoundPlayer()",
    //         asMETHOD(ThriveGame, getGuiSoundPlayer),
    //         asCALL_THISCALL) < 0)
    // {
    //     ANGELSCRIPT_REGISTERFAIL;
    // }
    

    if(engine->RegisterObjectMethod("ThriveGame",
            "void startNewGame()",
            asMETHOD(ThriveGame, startNewGame),
            asCALL_THISCALL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }

    if(engine->RegisterObjectProperty("ThriveGame", "ObjectID m_backgroundPlane",
            asOFFSET(ThriveGame, m_backgroundPlane)) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }    
    
    // if(engine->RegisterObjectMethod("Client",
    //         "bool Connect(const string &in address, string &out errormessage)",
    //         asMETHODPR(Client, Connect, (const std::string&, std::string&), bool),
    //         asCALL_THISCALL) < 0)
    // {
    //     ANGELSCRIPT_REGISTERFAIL;
    // }

    if(engine->RegisterObjectType("CellStageWorld", 0, asOBJ_REF | asOBJ_NOCOUNT) < 0){
        ANGELSCRIPT_REGISTERFAIL;
    }

    if(!Leviathan::BindStandardWorldMethods<CellStageWorld>(engine, "CellStageWorld"))
        return false;

    ANGLESCRIPT_BASE_CLASS_CASTS_NO_REF(Leviathan::StandardWorld, "StandardWorld",
        CellStageWorld, "CellStageWorld");

    ANGLESCRIPT_BASE_CLASS_CASTS_NO_REF(Leviathan::GameWorld, "GameWorld",
        CellStageWorld, "CellStageWorld");

    if(engine->RegisterObjectMethod("ThriveGame",
            "CellStageWorld@ getCellStage()",
            asMETHOD(ThriveGame, getCellStage),
            asCALL_THISCALL) < 0)
    {
        ANGELSCRIPT_REGISTERFAIL;
    }
    
    return true;
}

void ThriveGame::RegisterCustomScriptTypes(asIScriptEngine* engine,
    std::map<int, std::string> &typeids)
{
    typeids.insert(std::make_pair(engine->GetTypeIdByDecl("ThriveGame"), "ThriveGame"));
    typeids.insert(std::make_pair(engine->GetTypeIdByDecl("CellStageWorld"),
            "CellStageWorld"));
    typeids.insert(std::make_pair(engine->GetTypeIdByDecl("PlayerData"),
            "PlayerData")); 
    typeids.insert(std::make_pair(engine->GetTypeIdByDecl("LockedMap"),
            "LockedMap")); 
}



