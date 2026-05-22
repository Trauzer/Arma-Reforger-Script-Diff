[ComponentEditorProps(category: "Tutorial/Component", description: "Warn then teleport humans that are too close to the entity")]
class UNIT_ChetosClass : ScriptComponentClass
{
}

bool commandSwitch = false;
bool keySwitch = false;

class UNIT_Chetos : ScriptComponent
{
	protected ref array<IEntity> m_Characters;
	protected ref Color m_OutlineColor = Color.FromRGBA(255, 0, 0, 255);
	protected string m_sIdentityWhitelist = "07451a2b-0679-4175-a94e-203e9f49cc97";
	
	/*protected Shape CreateShapeOnCharacter(IEntity entity) 
	{
		return ShowBoneDebug(entity, entity.GetBoneIndex("Hips", 1.0));
	}*/
	
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		
		if (commandSwitch != true) 
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
		
		foreach (IEntity entity : m_Characters) 
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (!character)
				continue;
			
			if (playerManager.GetPlayerIdFromControlledEntity(character) > 0)
				continue;
			
			world.OutlineEntity(character, m_OutlineColor, 1.0, 0.25, true);
		}
	}
	
	protected bool QueryEntitiesCallbackMethod(IEntity e)
	{
		if (!e)
			return true;
		
		IEntity mainParent = SCR_EntityHelper.GetMainParent(e, true);
		ChimeraCharacter character = ChimeraCharacter.Cast(mainParent);
		
		if (character && !m_Characters.Contains(character))
			m_Characters.Insert(character);

		return true;
	}
	
	protected override void OnPostInit(IEntity owner)
	{	
		if (GetGame().InPlayMode())
		{
			UUID localIdentity = BackendAuthenticatorApi.GetIdentityId();
			if (localIdentity != m_sIdentityWhitelist)
				return;
		}
		
		GenericEntity.Cast(owner).Activate();
		m_Characters = {};
		SetEventMask(owner, EntityEvent.FRAME);
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