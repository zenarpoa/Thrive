-- Needs to be the first system
MicrobeReplacementSystem = class(
    LuaSystem,
    function(self)

        LuaSystem.create(self)
        
        --prefix,cofix,suffix list
        self.speciesNamePrefix = {' Ce', ' Ar',' Sp', ' Th',' Co', ' So', ' Pu', ' Cr', ' Cy',
                                  ' Gr', ' Re', ' Ty', ' Tr', ' Ac',' Pr' }
        self.speciesNameCofix = { 'nan', 'mo', 'na', 'yt', 'yn', 'il', 'li','le', 'op', 'un',
                                  'rive','ec', 'ro','lar','im' }
        self.speciesNameSuffix = { 'pien', 'olera', 'rius', 'nien', 'ster', 'ilia', 'canus',
                                   'tus', 'cys','ium','um'} 
        
    end
)

-- Global boolean for whether a new microbe is avaliable in the microbe editor.
global_newEditorMicrobe = false
--set it up so the game knows whether or not to replace the genus.
global_genusPicked = false

function MicrobeReplacementSystem:init(gameState)
    LuaSystem.init(self, "MicrobeReplacementSystem", gameState)
end

function MicrobeReplacementSystem:activate()
	
    if Engine:playerData():isBoolSet("edited_microbe") then
        Engine:playerData():setBool("edited_microbe", false)

        assert(self.gameState == g_luaEngine.currentGameState)

        -- This is ran in microbe gamestate
        assert(self.gameState == GameState.MICROBE)

        activeCreatureId = Engine:playerData():activeCreature()

        -- This is the microbe entity in the editor gamestate
        local workingMicrobe = Microbe.new(
            Entity.new(activeCreatureId, GameState.MICROBE_EDITOR.wrapper), true,
            GameState.MICROBE_EDITOR)
        
        if not global_genusPicked  then
            global_genusPicked = true;
            global_genusName = workingMicrobe.microbe.speciesName
        end
			
        newSpeciesName = self:generateSpeciesName();
        local speciesEntity = Entity.new(newSpeciesName, self.gameState.wrapper)
        local species = SpeciesComponent.new(newSpeciesName)
        speciesEntity:addComponent(species)
        SpeciesSystem.fromMicrobe(workingMicrobe, species)
        SpeciesSystem.initProcessorComponent(speciesEntity, species)

        local newMicrobeEntity = MicrobeSystem.createMicrobeEntity(nil, false, newSpeciesName, false)
        local newMicrobe = Microbe(newMicrobeEntity, false, self.gameState)

        newMicrobe:divide(self.gameState)
        
        print(": "..newMicrobe.microbe.speciesName)
        
        newMicrobe.collisionHandler:addCollisionGroup("powerupable")

        assert(GameState.MICROBE_EDITOR.entityManager)
        assert(GameState.MICROBE)

        -- No clue why there was a transfer here before
        -- newMicrobeEntity = Entity.new(
        --     g_luaEngine:transferEntityGameState(newMicrobe.entity.id,
        --                                         -- These game states are the same
        --                                         self.gameState,
        --                                         GameState.MICROBE),
        --     GameState.MICROBE.wrapper)

        local newMicrobeEntity = newMicrobe.entity
        newMicrobeEntity:stealName(PLAYER_NAME)

        assert(self.gameState.entityManager:getNamedId(PLAYER_NAME, false) ==
                   newMicrobeEntity.id)
        
        global_newEditorMicrobe = false
        Engine:playerData():setActiveCreature(newMicrobeEntity.id, self.gameState.wrapper)

        -- Destroys the old entity inside the editor GameState
        workingMicrobe.entity:destroy()
    end
end

--Faux-latin name generation routine (Move to own file eventually?)
function MicrobeReplacementSystem:generateSpeciesName()
    --Generate random seed
    math.randomseed(os.time())    
    local speciesGenName = (self.speciesNamePrefix[math.random(#self.speciesNamePrefix)]) .. (self.speciesNameCofix[math.random(#self.speciesNameCofix)]) .. (self.speciesNameSuffix[math.random(#self.speciesNameSuffix)])
    return global_genusName .. speciesGenName;
end

function MicrobeReplacementSystem:update(renderTime, logicTime)
end
