class UNIT_XRayOutlineComponentClass : ScriptComponentClass
{
}

class UNIT_XRayOutlineComponent : ScriptComponent
{
    [Attribute("1")]
    protected bool m_bEnabled;

    [Attribute("1")]
    protected bool m_bEnemiesOnly;

    [Attribute("150", desc: "Max range in meters")]
    protected float m_fMaxDistance;

    [Attribute("", desc: "Allowed BI Identity IDs separated by ';'. Empty means no restriction")]
    protected string m_sIdentityWhitelist;

    protected ref Color m_OutlineColor = Color.FromRGBA(255, 0, 0, 255);
    protected ref array<string> m_aWhitelistedIdentityIds = {};
    protected ref array<IEntity> m_aNearbyCharactersQuery = {};

    //------------------------------------------------------------------------------------------------
    override void OnPostInit(IEntity owner)
    {
        ParseIdentityWhitelist();

        SetEventMask(owner, EntityEvent.FRAME);
        GenericEntity.Cast(owner).Activate();
    }

    //------------------------------------------------------------------------------------------------
    override void EOnFrame(IEntity owner, float timeSlice)
    {
        if (!m_bEnabled || System.IsConsoleApp())
            return;

        if (!CanLocalPlayerUseFeature())
            return;

        World world = GetGame().GetWorld();
        PlayerManager playerManager = GetGame().GetPlayerManager();
        if (!world || !playerManager)
            return;

        int localId = SCR_PlayerController.GetLocalPlayerId();
        IEntity localEntity = playerManager.GetPlayerControlledEntity(localId);
        Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
        array<IEntity> outlinedEntities = {};

        array<int> players = {};
        playerManager.GetPlayers(players);

        foreach (int playerId : players)
        {
            if (playerId == localId)
                continue;

            SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
            if (!character)
                continue;

            TryOutlineCharacter(world, localEntity, localFaction, character, outlinedEntities);
        }

        // AIWorld can be unavailable or empty on some client contexts, so keep this as best-effort only.
        AIWorld aiWorld = GetGame().GetAIWorld();
        if (aiWorld)
        {
            array<AIAgent> aiAgents = {};
            aiWorld.GetAIAgents(aiAgents);

            foreach (AIAgent aiAgent : aiAgents)
            {
                if (!aiAgent)
                    continue;

                SCR_ChimeraCharacter aiCharacter = SCR_ChimeraCharacter.Cast(aiAgent.GetControlledEntity());
                if (!aiCharacter || playerManager.GetPlayerIdFromControlledEntity(aiCharacter) > 0)
                    continue;

                TryOutlineCharacter(world, localEntity, localFaction, aiCharacter, outlinedEntities);
            }
        }

        vector queryCenter;
        if (localEntity)
        {
            queryCenter = localEntity.GetOrigin();
        }
        else
        {
            vector cam[4];
            world.GetCurrentCamera(cam);
            queryCenter = cam[3];
        }

        m_aNearbyCharactersQuery.Clear();
        world.QueryEntitiesBySphere(queryCenter, m_fMaxDistance, CollectNearbyCharacters, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);

        foreach (IEntity nearbyEntity : m_aNearbyCharactersQuery)
        {
            SCR_ChimeraCharacter nearbyCharacter = SCR_ChimeraCharacter.Cast(nearbyEntity);
            if (!nearbyCharacter || playerManager.GetPlayerIdFromControlledEntity(nearbyCharacter) > 0)
                continue;

            TryOutlineCharacter(world, localEntity, localFaction, nearbyCharacter, outlinedEntities);
        }
    }

    //------------------------------------------------------------------------------------------------
    protected bool CollectNearbyCharacters(IEntity entity)
    {
        if (entity)
            m_aNearbyCharactersQuery.Insert(entity);

        return true;
    }

    //------------------------------------------------------------------------------------------------
    protected void TryOutlineCharacter(World world, IEntity localEntity, Faction localFaction, SCR_ChimeraCharacter character, inout notnull array<IEntity> outlinedEntities)
    {
        if (outlinedEntities.Contains(character))
            return;

        if (m_bEnemiesOnly && localFaction && character.GetFaction() == localFaction)
            return;

        if (localEntity && vector.DistanceSq(localEntity.GetOrigin(), character.GetOrigin()) > m_fMaxDistance * m_fMaxDistance)
            return;

        if (!IsOccludedFromCamera(world, localEntity, character))
            return;

        world.OutlineEntity(character, m_OutlineColor, 1.0, 0.25, true);
        outlinedEntities.Insert(character);
    }

    //------------------------------------------------------------------------------------------------
    protected void ParseIdentityWhitelist()
    {
        m_aWhitelistedIdentityIds.Clear();

        if (!m_sIdentityWhitelist)
            return;

        array<string> tokens = {};
        m_sIdentityWhitelist.Split(";", tokens, true);

        foreach (string token : tokens)
        {
            token.Replace(" ", "");

            if (!token || !UUID.IsUUID(token))
                continue;

            if (!m_aWhitelistedIdentityIds.Contains(token))
                m_aWhitelistedIdentityIds.Insert(token);
        }
    }

    //------------------------------------------------------------------------------------------------
    protected bool CanLocalPlayerUseFeature()
    {
        // Empty whitelist means feature is available for everyone.
        if (m_aWhitelistedIdentityIds.Count() == 0)
            return true;

        UUID localIdentity = BackendAuthenticatorApi.GetIdentityId();
        if (localIdentity.IsNull())
            return false;

        return m_aWhitelistedIdentityIds.Contains(localIdentity);
    }

    //------------------------------------------------------------------------------------------------
    protected bool IsOccludedFromCamera(World world, IEntity localEntity, IEntity target)
    {
        vector cam[4];
        world.GetCurrentCamera(cam);

        TraceParam trace = new TraceParam();
        trace.Start = cam[3];
        trace.End = target.GetOrigin() + Vector(0, 1.5, 0);
        trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS | TraceFlags.ANY_CONTACT;
        trace.Exclude = localEntity;
        trace.TraceEnt = null;

        float traveled = world.TraceMove(trace, null);
        if (traveled >= 1)
            return false;

        if (!trace.TraceEnt)
            return true;

        return trace.TraceEnt.GetRootParent() != target.GetRootParent();
    }
}
