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

        array<int> players = {};
        playerManager.GetPlayers(players);

        foreach (int playerId : players)
        {
            if (playerId == localId)
                continue;

            SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
            if (!character)
                continue;

            if (m_bEnemiesOnly && localFaction && character.GetFaction() == localFaction)
                continue;

            if (localEntity && vector.DistanceSq(localEntity.GetOrigin(), character.GetOrigin()) > m_fMaxDistance * m_fMaxDistance)
                continue;

            if (!IsOccludedFromCamera(world, localEntity, character))
                continue;

            world.OutlineEntity(character, m_OutlineColor, 1.0, 0.25, true);
        }
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
