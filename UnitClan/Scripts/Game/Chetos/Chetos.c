[ComponentEditorProps(category: "Tutorial/Component", description: "Warn then teleport humans that are too close to the entity")]
class UNIT_ChetosClass : ScriptComponentClass
{
}

bool commandSwitch = false; //TODO: Move it to class

class UNIT_Chetos : ScriptComponent
{
	protected ref map<IEntity, bool> m_Characters;
	protected ref Color m_OutlineEnemyColor = Color.FromRGBA(255, 0, 0, 255);
	protected ref Color m_OutlineFriendlyColor = Color.FromRGBA(0, 0, 255, 255);
	protected string m_sIdentityWhitelist = "07451a2b-0679-4175-a94e-203e9f49cc97"; //TODO: Permission/whitelist system
	protected bool keySwitch = false;
	protected Faction m_PlayerFaction;

	
	override void OnPostInit(IEntity owner)
	{	
		super.EOnActivate(owner);
			
		Game game = GetGame();
		
		if (game.InPlayMode())
		{
			UUID localIdentity = BackendAuthenticatorApi.GetIdentityId();
			if (localIdentity != m_sIdentityWhitelist)
				return;
		}
		//TODO: Admin check		
		
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
    	if (gm)
        	gm.GetOnPlayerRegistered().Insert(OnPlayerRegistered);
			
		game.GetInputManager().AddActionListener("RHS_CharacterOpenRadial", EActionTrigger.DOWN, KeyPress); //Change dependency
		
		GenericEntity.Cast(owner).Activate();
		m_Characters = new map<IEntity, bool>();
		SetEventMask(owner, EntityEvent.FRAME);
	}
	
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		
		if (commandSwitch != true) 
		{
			return;
		}
		
		if (keySwitch != true)
		{
			return;
		}
		
		m_Characters.Clear();
		World world = owner.GetWorld();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		IEntity localEntity = playerManager.GetPlayerControlledEntity(SCR_PlayerController.GetLocalPlayerId());
		
		vector center;
		if (localEntity)
		{
			center = localEntity.GetOrigin();
		}
		else
		{
			vector cam[4];
			world.GetCurrentCamera(cam);
			center = cam[3];
		}
		
		world.QueryEntitiesBySphere(center, 250.0, QueryEntitiesCallbackMethod, null, EQueryEntitiesFlags.DYNAMIC);
		
		//world.GetActiveEntities(m_Characters);
		
		foreach (IEntity entity, bool isFriendly : m_Characters) 
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (!character)
				continue;
			
			if (playerManager.GetPlayerIdFromControlledEntity(character) > 0)
				continue;
			
			if (isFriendly) 
			{
				world.OutlineEntity(character, m_OutlineFriendlyColor, 1.0, 0.25, true);
			}
			else
			{
				world.OutlineEntity(character, m_OutlineEnemyColor, 1.0, 0.25, true);
			}
		}
	}
	
	protected bool QueryEntitiesCallbackMethod(IEntity e)
	{
		if (!e)
			return true;
		
		IEntity mainParent = SCR_EntityHelper.GetMainParent(e, true);
		ChimeraCharacter character = ChimeraCharacter.Cast(mainParent);

		if (character && !m_Characters.Contains(character))
		{
			bool isFriendly = true;

			Faction faction = SCR_Faction.GetEntityFaction(character);
			//SCR_FactionAffiliationComponent factionComp = SCR_FactionAffiliationComponent.Cast(character.GetCharacterController().FindComponent(SCR_FactionAffiliationComponent));
			//Faction faction = factionComp.GetAffiliatedFaction();
			
			if (m_PlayerFaction.IsFactionEnemy(faction))
			{
				isFriendly = false;
			}
				
			m_Characters.Insert(character, isFriendly);
		}

		return true;
	}
	
	protected void OnPlayerRegistered(int playerId)
	{
	    int localId = SCR_PlayerController.GetLocalPlayerId();
	    if (playerId != localId)
	        return;
	
	    SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
	    if (!pc)
	        return;
	
	    SCR_PlayerFactionAffiliationComponent m_PlayerFactionComp = SCR_PlayerFactionAffiliationComponent.Cast(pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
	    if (!m_PlayerFactionComp)
	        return;
	
	    m_PlayerFaction = m_PlayerFactionComp.GetAffiliatedFaction();
	    m_PlayerFactionComp.GetOnPlayerFactionChangedInvoker().Insert(OnPlayerFactionChanged);
	
	    // opcjonalnie: odpinamy sie od OnPlayerRegistered po pierwszym trafieniu
	    SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
	    if (gm)
	        gm.GetOnPlayerRegistered().Remove(OnPlayerRegistered);
	}
	
	protected void OnPlayerFactionChanged(SCR_PlayerFactionAffiliationComponent comp, Faction previous, Faction current)
	{
		Print("[UNIT]" + current.GetFactionIdentity(), LogLevel.NORMAL);
	    m_PlayerFaction = current;
	}
	
	void KeyPress(float value, EActionTrigger reason)
	{
		if (!keySwitch)
		{
			keySwitch = true;
		} 
		else
		{
			keySwitch = false;
		}
	}
	
	void ~UNIT_Chetos() 
	{
		GetGame().GetInputManager().RemoveActionListener("RHS_CharacterOpenRadial", EActionTrigger.DOWN, KeyPress);
		
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
	    if (gm)
	        gm.GetOnPlayerRegistered().Remove(OnPlayerRegistered);
	
	    //if (m_PlayerFactionComp)
	    //    m_PlayerFactionComp.GetOnPlayerFactionChangedInvoker().Remove(OnPlayerFactionChanged);
	}
}

class Chetos_Command : ScrServerCommand
{
	// Specify keyword of command
	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return "setwh";
	}

	// Run command server-side
	//------------------------------------------------------------------------------------------------
	override bool IsServerSide()
	{
		return false;
	}

	// Set requirement to admin permission via RCON
	//------------------------------------------------------------------------------------------------
	/*override int RequiredRCONPermission()
	{
		return ERCONPermissions.PERMISSIONS_ADMIN;
	}*/

	// Set requirement to be logged in administrator for chat command
	//------------------------------------------------------------------------------------------------
	override int RequiredChatPermission()
	{
		return EPlayerRole.NONE;
	}

	//------------------------------------------------------------------------------------------------
	/*protected ScrServerCmdResult ReloadRoles(array<string> argv, int playerId = -1)
	{
		WCS_Commands_AdministrationComponent.ReloadRoles();

		return ScrServerCmdResult("Admins reloaded", EServerCmdResultType.OK);
	}*/

	// Handle Chat command on server
	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	// Handle Chat command on client
	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId)
	{
		if (!commandSwitch) 
		{
			commandSwitch = true;
		} 
		else 
		{
			commandSwitch = false;
		}
		
		return ScrServerCmdResult("State: " + commandSwitch, EServerCmdResultType.OK);
	}

	// Handle RCON command on server
	//------------------------------------------------------------------------------------------------
	/*override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ReloadRoles(argv);
	}*/

	// Handle Pending command
	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}
}