enum Bacon_GunBuilderUI_ResponseType {
	ADD_INVENORY_ITEM,
	SWAP_ATTACHMENT,
	LOADOUT
}

enum Bacon_GunBuilderUI_ActionType {
	ADD_ITEM,
	REMOVE_ITEM,
	REPLACE_ITEM,
	GET_LOADOUTS,
	SAVE_LOADOUT,
	CLEAR_LOADOUT,
	APPLY_LOADOUT,
	GET_ADMIN_LOADOUTS,
	SAVE_LOADOUT_ADMIN,
	APPLY_LOADOUT_ADMIN,
	SET_AI_LOADOUT_ADMIN,
	CLEAR_LOADOUT_ADMIN,
	CHANGE_VISUAL_IDENTITY,
	CHANGE_SOUND_IDENTITY
}

// requests
class Bacon_GunBuilderUI_Network_Request {
	Bacon_GunBuilderUI_ActionType actionType;
	
	string Repr() {
		return string.Format("action: %1", SCR_Enum.GetEnumName(Bacon_GunBuilderUI_ActionType, actionType));
	}
}
sealed class Bacon_GunBuilderUI_Network_StorageRequest: Bacon_GunBuilderUI_Network_Request {
	RplId arsenalEntityRplId;
	RplId storageRplId;
	int storageSlotId = -1;
	ResourceName prefab = "";
	
	override string Repr() {
		return string.Format("action: %1, rplId: %2, slotId: %3, prefab: %4", SCR_Enum.GetEnumName(Bacon_GunBuilderUI_ActionType, actionType), storageRplId, storageSlotId, prefab);
	}
}
sealed class Bacon_GunBuilderUI_Network_IdentityChangeRequest: Bacon_GunBuilderUI_Network_Request {
	// RplId identityComponentRplId;
	// string factionKey;
	int identityIndex;
}
sealed class Bacon_GunBuilderUI_Network_LoadoutRequest: Bacon_GunBuilderUI_Network_Request {
	int loadoutSlotId = -1;
	RplId arsenalComponentRplId;
}

// responses
sealed class Bacon_GunBuilderUI_Network_Response {
	bool success;
	string message;
	ref Bacon_GunBuilderUI_Network_Request request;
}

sealed class Bacon_GunBuilderUI_PlayerControllerComponentClass: ScriptComponentClass {};

sealed class Bacon_GunBuilderUI_PlayerControllerComponent: ScriptComponent {
	ref ScriptInvoker m_OnSwapRequestResponseRplId = new ScriptInvoker();
	ref ScriptInvoker m_OnResponse_Storage = new ScriptInvoker();
	ref ScriptInvoker m_OnResponse_Loadout = new ScriptInvoker();
	
	Bacon_GunBuilder_LoadoutStorageComponent m_LoadoutStorageComponent;
	SCR_MapMarkerEntrySquadLeader m_MapMarkerEntrySquadLeader;
	
	PlayerManager m_playerManager;
	SCR_BaseGameMode m_gameMode;
	PlayerController m_PC;
	
	static Bacon_GunBuilderUI_PlayerControllerComponent LocalInstance;
	static Bacon_GunBuilderUI_PlayerControllerComponent ServerInstance;
	
	protected SCR_ArsenalManagerComponent m_arsenalManager;

	static ref array<ref Bacon_GunBuilder_PlayerLoadout> AdminLoadoutMetadata = {};

	override void OnPostInit(IEntity owner) {
		SetEventMask(owner, EntityEvent.INIT);
		m_PC = PlayerController.Cast(owner);
	};
	
	override void EOnInit(IEntity owner) {
		if (m_PC.GetPlayerId() == SCR_PlayerController.GetLocalPlayerId()) {
			LocalInstance = this;
		}
		
		m_playerManager = GetGame().GetPlayerManager();
		
		if (!Replication.IsServer()) {
			GetGame().GetCallqueue().CallLater(AskForLoadouts, 100, false);
			return;
		}
		
		if (SCR_PlayerController.GetLocalPlayerId() == 0)
			ServerInstance = this;
		
		m_gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!m_gameMode) {
			Print("Bacon_GunBuilderUI_PlayerControllerComponent.EOnInit | Failed to obtain game mode information", LogLevel.WARNING);
			return;
		}

		m_LoadoutStorageComponent = Bacon_GunBuilder_LoadoutStorageComponent.Cast(GetGame().GetGameMode().FindComponent(Bacon_GunBuilder_LoadoutStorageComponent));
		UpdateServerLoadouts();
		
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerManager) {
			Print("Bacon_GunBuilderUI_PlayerControllerComponent.EOnInit | SCR_MapMarkerManagerComponent is null", LogLevel.WARNING);
			return;
		}
			
		SCR_MapMarkerConfig markerConfig = markerManager.GetMarkerConfig();
		if (!markerConfig) {
			Print("Bacon_GunBuilderUI_PlayerControllerComponent.EOnInit | SCR_MapMarkerConfig is null", LogLevel.WARNING);
			return;
		}
			
		m_MapMarkerEntrySquadLeader = SCR_MapMarkerEntrySquadLeader.Cast(markerConfig.GetMarkerEntryConfigByType(SCR_EMapMarkerType.SQUAD_LEADER));
		
		SCR_ArsenalManagerComponent.GetArsenalManager(m_arsenalManager);
	};

	void UpdateServerLoadouts() {
		AdminLoadoutMetadata.Clear();
		m_LoadoutStorageComponent.GetPlayerLoadoutMetadata(0, "", "admin", AdminLoadoutMetadata, true);

		// Rpc(RpcDo_UpdateLoadouts, loadoutsJson);
	}
	
	void AskForLoadouts() {
		Rpc(RpcAsk_LoadoutsPlease);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_LoadoutsPlease() {
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", AdminLoadoutMetadata);
		
		string loadoutsJson = saveContext.ExportToString();
		
		Rpc(RpcDo_UpdateLoadoutsOwner, loadoutsJson);
	}
	
	void BroadcastLoadoutChange() {
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", AdminLoadoutMetadata);
		
		string loadoutsJson = saveContext.ExportToString();
		Rpc(RpcDo_UpdateLoadoutsBroadcast, loadoutsJson);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_UpdateLoadoutsOwner(string json) {
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(json);
		loadContext.ReadValue("", AdminLoadoutMetadata);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcDo_UpdateLoadoutsBroadcast(string json) {
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(json);
		loadContext.ReadValue("", AdminLoadoutMetadata);
	}
	
	SCR_InventoryStorageManagerComponent GetPlayerInventoryManager(int playerId) {
		IEntity character = m_playerManager.GetPlayerControlledEntity(playerId);
		if (!character)
			return null;
		
		return SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
	}
	
	BaseInventoryStorageComponent GetPlayerStorage(Bacon_GunBuilderUI_StorageType storageType, int playerId) {
		IEntity character = m_playerManager.GetPlayerControlledEntity(playerId);
		
		if (storageType == Bacon_GunBuilderUI_StorageType.CHARACTER_LOADOUT)
			return BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		
		if (storageType == Bacon_GunBuilderUI_StorageType.CHARACTER_WEAPON)
			return BaseInventoryStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));
		
		return null;
	}
	
	BaseInventoryStorageComponent GetPlayerWeaponStorage(int playerId, int weaponSlotId) {
		IEntity character = m_playerManager.GetPlayerControlledEntity(playerId);

		BaseInventoryStorageComponent weaponStorage = BaseInventoryStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));
		return BaseInventoryStorageComponent.Cast(weaponStorage.GetSlot(weaponSlotId).GetAttachedEntity().FindComponent(BaseInventoryStorageComponent));

	}

	void RequestAction(Bacon_GunBuilderUI_Network_StorageRequest request) {
		// request.Pack();	
		
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", request);

		string requestString = saveContext.ExportToString();
		
		Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RequestAction | Sending request: %1", requestString), LogLevel.DEBUG);
		// Rpc(RpcAsk_RequestAction, SCR_PlayerController.GetLocalPlayerId(), requestString);
		Rpc(RpcAsk_RequestAction, requestString);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestAction(string requestJson) {
		int playerId = m_PC.GetPlayerId();
		
		Bacon_GunBuilderUI_Network_StorageRequest request = new Bacon_GunBuilderUI_Network_StorageRequest();
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(requestJson);
		loadContext.ReadValue("", request);
		// request.ExpandFromRAW(requestJson);
		
		Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RpcAsk_RequestAction | Processing request from player %1: %2", playerId, requestJson), LogLevel.DEBUG);
		
		if (request.actionType == Bacon_GunBuilderUI_ActionType.CHANGE_VISUAL_IDENTITY) {
			IEntity ent = m_PC.GetControlledEntity();
			if (!ent) {
				SendActionResponse(request, false, "Could not find controlled entity for this player");
				return;
			}

			IEntity arsenalEntity = Bacon_GunBuilderUI_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
			if (!arsenalEntity) {
				SendActionResponse(request, false, "Could not find arsenal entity in replication");
				return;
			}
			
			SCR_FactionAffiliationComponent arsenalFactionComp = SCR_FactionAffiliationComponent.Cast(arsenalEntity.FindComponent(SCR_FactionAffiliationComponent));
			if (!arsenalFactionComp) {
				SendActionResponse(request, false, "Could not find faction component of the Arsenal Entity");
				return;
			}
			
			Faction arsenalFaction = arsenalFactionComp.GetAffiliatedFaction();
			if (!arsenalFaction) {
				SendActionResponse(request, false, "Could not find faction of the Arsenal Entity");
				return;
			}
			
//			string factionKey = arsenalFaction.GetFactionKey();
//			if (factionKey.IsEmpty()) {
//				SendActionResponse(request, false, "Invalid arsenal faction");
//				return;
//			}
//			
//			FactionManager factionManager = GetGame().GetFactionManager();
//			if (!factionManager) {
//				SendActionResponse(request, false, "No faction manager");
//				return;
//			}
			
			FactionIdentity factionIdentity = arsenalFaction.GetFactionIdentity();
			if (!factionIdentity) {
				SendActionResponse(request, false, "Faction has no identity");
				return;
			}

			SCR_CharacterIdentityComponent identityComponent = SCR_CharacterIdentityComponent.Cast(ent.FindComponent(SCR_CharacterIdentityComponent));
			if (!identityComponent) {
				SendActionResponse(request, false, "No identity component on character");
				return;
			}
			
			Identity id = identityComponent.GetIdentity();
			VisualIdentity newVisualIdentity = factionIdentity.CreateVisualIdentity(request.storageSlotId);
			if (!newVisualIdentity) {
				SendActionResponse(request, false, "Creation of Visual Identity failed");
				return;
			}
			id.SetVisualIdentity(newVisualIdentity);
			
			identityComponent.SetIdentity(id);
			SendActionResponse(request, true, "Identity changed");
			return;
		}
		
		Managed entity = Replication.FindItem(request.storageRplId);
		if (!entity) {
			SendActionResponse(request, false, "Invalid entity provided");
			return;
		}
		
		BaseInventoryStorageComponent editedStorage = BaseInventoryStorageComponent.Cast(entity);
		if (!editedStorage) {
			SendActionResponse(request, false, "Provided entity is no a storage component");
			return;
		}
		
		SCR_InventoryStorageManagerComponent storageManager = GetPlayerInventoryManager(playerId);
		if (!storageManager) {
			SendActionResponse(request, false, "Character storage manager not found");
			return;
		}

		switch (request.actionType) {
			case Bacon_GunBuilderUI_ActionType.ADD_ITEM: {
				Action_AddItemToStorage(request, editedStorage, storageManager);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.REMOVE_ITEM: {
				// Action_RemoveItemFromStorage(request, editedStorage, storageManager);
				Action_TransferItemToStorage(request, editedStorage, storageManager);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.REPLACE_ITEM: {
				Action_ReplaceItemInSlotWithPrefab(request, editedStorage, storageManager);
				break;
			}
		}
	}
	
	bool CanAffordItem(Bacon_GunBuilderUI_Network_StorageRequest request, out float cost) {
		if (!SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled()) {
			return true;
		}

		IEntity arsenalEntity = Bacon_GunBuilderUI_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
		if (!arsenalEntity) {
			SendActionResponse(request, false, "Cant find Arsenal Entity");
			return false;
		}

		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
		if (!arsenalComponent) {
			SendActionResponse(request, false, "Cant find Arsenal Component in Entity");
			return false;
		}
		
		cost = Bacon_GunBuilderUI_Helpers.GetItemSupplyCost(arsenalComponent, request.prefab);
		if (cost < 0.1)
			return true;

		SCR_ResourceComponent resourceComponent	= SCR_ResourceComponent.FindResourceComponent(arsenalEntity);
		if (!resourceComponent) {
			SendActionResponse(request, false, "Cant find Resource Component in Arsenal Entity");
			return false;
		}
		
		SCR_ResourceConsumer consumer = resourceComponent.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!consumer) {
			SendActionResponse(request, false, "Cant find Resource Consumer in Arsenal Resource Component");
			return false;
		}
		
		if (resourceComponent) {
			// auto resourceInventoryComponent = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetOwner().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));

			cost *= consumer.GetBuyMultiplier();
			//bool success = resourceInventoryComponent.Bacon_GunBuilder_TryPerformResourceConsumption(consumer, cost);
			SCR_ResourceConsumtionResponse resp = consumer.RequestConsumtion(cost);
			bool success = resp.GetReason() == EResourceReason.SUFFICIENT;
			
			if (!success) {
			// if (resp.GetReason() != EResourceReason.SUFFICIENT) {
				SendActionResponse(request, false, string.Format("Not enough supplies (cost: %1)", cost));
			}
			
			return success;
		}
		
		return true;
	}
	
	bool CanAffordLoadout(SCR_ArsenalComponent arsenalComponent, Bacon_GunBuilderUI_Network_LoadoutRequest request, float cost) {
		if (!arsenalComponent) {
			SendActionResponse(request, false, "Cant find Arsenal Component in Entity");
			return false;
		}
		
		SCR_ResourceComponent resourceComponent	= SCR_ResourceComponent.FindResourceComponent(arsenalComponent.GetOwner());
		if (!Bacon_GunBuilderUI_Helpers.AreSuppliesEnabled(resourceComponent))
			return true;

		SCR_ResourceConsumer consumer = resourceComponent.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!consumer) {
			SendActionResponse(request, false, "Cant find Resource Consumer in Arsenal Resource Component");
			return false;
		}
		
		if (resourceComponent) {
			SCR_ArsenalManagerComponent arsenalManager;
			float multiplier = 1;
			if (SCR_ArsenalManagerComponent.GetArsenalManager(arsenalManager)) {
				multiplier = arsenalManager.GetCalculatedLoadoutSpawnSupplyCostMultiplier();
				if (multiplier < 1)
					multiplier = 1;
			}
			
			// auto resourceInventoryComponent = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetOwner().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));

			cost *= multiplier;
			// bool success = resourceInventoryComponent.Bacon_GunBuilder_TryPerformResourceConsumption(consumer, cost);
			SCR_ResourceConsumtionResponse resp = consumer.RequestConsumtion(cost);
			bool success = resp.GetReason() == EResourceReason.SUFFICIENT;
			
			if (!success) {
				SendActionResponse(request, false, string.Format("Not enough supplies (cost: %1)", cost));
				// SendActionResponse(request, false, "Not enough supply");
			}

			return success;
		}
		
		return true;
	}
	
	bool HasHighEnoughRank(Bacon_GunBuilderUI_Network_StorageRequest request) {
		// if (!SCR_GameModeCampaign.GetInstance())
		//	return true;
		
		if (m_arsenalManager && !m_arsenalManager.AreItemsRankLocked())
			return true;

		IEntity arsenalEntity = Bacon_GunBuilderUI_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
		if (!arsenalEntity) {
			SendActionResponse(request, false, "Cant find Arsenal Entity");
			return false;
		}
		
		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
		if (!arsenalComponent) {
			SendActionResponse(request, false, "Cant find Arsenal Component in Entity");
			return false;
		}

		SCR_ECharacterRank playerRank = SCR_CharacterRankComponent.GetCharacterRank(SCR_PlayerController.Cast(GetOwner()).GetControlledEntity());
		SCR_ECharacterRank rankRequired = Bacon_GunBuilderUI_Helpers.GetItemRequiredRank(arsenalComponent, request.prefab);

		if (playerRank == SCR_ECharacterRank.INVALID || rankRequired == SCR_ECharacterRank.INVALID)
			return true;
		
		if (playerRank < rankRequired) {
			string playerAsString = typename.EnumToString(SCR_ECharacterRank, playerRank);
			playerAsString.ToLower();
			string requiredAsString = typename.EnumToString(SCR_ECharacterRank, rankRequired);
			requiredAsString.ToLower();
			
			SendActionResponse(request, false, string.Format("%1 rank required (you are %2)", requiredAsString, playerAsString));
			return false;
		}
		
		return true;
	}
	
	void Action_AddItemToStorage(Bacon_GunBuilderUI_Network_StorageRequest request, BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent storageManager) {
		// for things like the alice vest we need to use different logic
		// this is stupid
		
//		ClothNodeStorageComponent loadoutCloth = ClothNodeStorageComponent.Cast(storage);
//		if (!loadoutCloth) {
//			Bacon_GunBuilderUI_InvCb_UIResponse cb = new Bacon_GunBuilderUI_InvCb_UIResponse();
//			
//			cb.messageOk = messageOk;
//			cb.messageFailed = "Failed to add prefab into storage";
//			cb.component = this;
//			cb.request = request;
//			
//			storageManager.TrySpawnPrefabToStorage(request.prefab, storage, request.storageSlotId, EStoragePurpose.PURPOSE_ANY, cb); 
//			return; 
//		}
		
		if (!HasHighEnoughRank(request))
			return;
		
		// IEntity itemEntity = Bacon_GunBuilderUI_Helpers.PrepareTemporaryEntity(request.prefab);
		IEntity itemEntity = Bacon_GunBuilderUI_Helpers.PrepareTemporaryEntityAtCoords(request.prefab, storageManager.GetOwner().GetOrigin());
		if (!itemEntity) {
			SendActionResponse(request, false, "Failed to spawn temporary entity"); return; }

		BaseInventoryStorageComponent appropriateStorage = storageManager.FindStorageForInsert(itemEntity, storage, EStoragePurpose.PURPOSE_ANY);
		if (!appropriateStorage) {
			SendActionResponse(request, false, "Failed to find suitable storage");
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
			return;
		}
		
		float cost;
		if (!CanAffordItem(request, cost))
			return;
		
		string messageOk = "Item added";
		if (cost > 0) {
			messageOk = string.Format("%1 (cost: %2 supply)", messageOk, cost);
		}
		
		Bacon_GunBuilderUI_InvCb_DeleteTemporaryEntityOnFailure deleteCb = new Bacon_GunBuilderUI_InvCb_DeleteTemporaryEntityOnFailure();
			
		deleteCb.messageOk = messageOk;
		deleteCb.messageFailed = "Failed to add prefab into substorage";
		deleteCb.temporaryEntity = itemEntity;
		deleteCb.component = this;
		deleteCb.request = request;

		storageManager.TryInsertItemInStorage(itemEntity, appropriateStorage, -1, deleteCb);
	}
	
	void Action_RemoveItemFromStorage(Bacon_GunBuilderUI_Network_StorageRequest request, BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent storageManager) {
		InventoryStorageSlot slot = storage.GetSlot(request.storageSlotId);
		if (!slot) {
			SendActionResponse(request, false, "Requested slot does not exist");
			return;
		}
		IEntity attachedEntity = slot.GetAttachedEntity();
  		if (!attachedEntity) {
			SendActionResponse(request, false, "Provided entity is invalid");
			return;
		}
		
		Bacon_GunBuilderUI_InvCb_UIResponse cb = new Bacon_GunBuilderUI_InvCb_UIResponse();
			
		cb.messageOk = "Item removed";
		cb.messageFailed = "Failed to remove item from storage";
		cb.request = request;
		cb.component = this;
			
		storageManager.TryDeleteItem(attachedEntity, cb);
	}
	// test
	
	protected bool TryRefundItem(Bacon_GunBuilderUI_Network_StorageRequest request, IEntity attachedEntity, out float refund) {
		IEntity arsenalEntity = Bacon_GunBuilderUI_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
		if (!arsenalEntity) {
			SendActionResponse(request, false, "Invalid arsenal entity");
			return false;
		}

		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
		if (!arsenalComponent) {
			SendActionResponse(request, false, "Invalid arsenal component");
			return false;
		}
		
		refund = Math.Clamp(SCR_ArsenalManagerComponent.GetItemRefundAmount(attachedEntity, arsenalComponent, false), 0, float.MAX);
		
		InventoryItemComponent inventoryItemComponent	= InventoryItemComponent.Cast(attachedEntity.FindComponent(InventoryItemComponent));
		SCR_ResourceComponent resourceComponent			= SCR_ResourceComponent.FindResourceComponent(arsenalEntity);
		if (resourceComponent) {
			auto resourceInventoryComponent = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetOwner().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
			resourceInventoryComponent.RpcAsk_ArsenalRefundItem(Replication.FindId(resourceComponent), Replication.FindId(inventoryItemComponent), EResourceType.SUPPLIES);
		}
		
		return true;
	}
	void TryRefundFixedCost(IEntity arsenalEntity, float cost) {
		SCR_ResourceComponent resourceComponent	= SCR_ResourceComponent.FindResourceComponent(arsenalEntity);
		if (!resourceComponent)
			return;
		
		SCR_ResourceGenerator generator	= resourceComponent.GetGenerator(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
		if (!generator)
			return;
		
		auto resourceInventoryComponent = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetOwner().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
		if (!resourceInventoryComponent)
			return;
		
		resourceInventoryComponent.Bacon_GunBuidler_TryPerformRefund(generator, cost);
	}
	
	void Action_TransferItemToStorage(Bacon_GunBuilderUI_Network_StorageRequest request, BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent storageManager) {
		InventoryStorageSlot slot = storage.GetSlot(request.storageSlotId);
		if (!slot)
			return SendActionResponse(request, false, "Requested slot does not exist");

		IEntity attachedEntity = slot.GetAttachedEntity();
  		if (!attachedEntity)
			return SendActionResponse(request, false, "Provided entity is invalid");

//		IEntity arsenalEntity = Bacon_GunBuilderUI_Helpers.GetEntityFromRplId(request.arsenalEntityRplId);
//		if (!arsenalEntity)
//			return SendActionResponse(request, false, "Invalid arsenal entity");
//
//		SCR_ArsenalComponent arsenalComponent = SCR_ArsenalComponent.Cast(arsenalEntity.FindComponent(SCR_ArsenalComponent));
//		if (!arsenalComponent)
//			return SendActionResponse(request, false, "Invalid arsenal component");
//		
//		float refund = Math.Clamp(SCR_ArsenalManagerComponent.GetItemRefundAmount(attachedEntity, arsenalComponent, false), 0, float.MAX);
//		
//		InventoryItemComponent inventoryItemComponent	= InventoryItemComponent.Cast(attachedEntity.FindComponent(InventoryItemComponent));
//		SCR_ResourceComponent resourceComponent			= SCR_ResourceComponent.FindResourceComponent(arsenalEntity);
//		if (resourceComponent) {
//			auto resourceInventoryComponent = SCR_ResourcePlayerControllerInventoryComponent.Cast(GetOwner().FindComponent(SCR_ResourcePlayerControllerInventoryComponent));
//			resourceInventoryComponent.RpcAsk_ArsenalRefundItem(Replication.FindId(resourceComponent), Replication.FindId(inventoryItemComponent), EResourceType.SUPPLIES);
//		}
		
		float refund;
		if (!TryRefundItem(request, attachedEntity, refund)) {
			return;
		}

		if (!attachedEntity) {
			string confirmation = "Item removed";
			if (refund > 0.1) {
				confirmation = string.Format("%1 (+%2 supply)", confirmation, refund);
			}
			return SendActionResponse(request, true, confirmation);
		}
		
		// if the entity still exists, means it could not be refunded, so delete it the usual way
//		if (!attachedEntity) {
//			string confirmation = "Item removed";
//			if (refund > 0.1) {
//				confirmation = string.Format("%1 (+%2 supply)", confirmation, refund);
//			}
//			return SendActionResponse(request, true, confirmation);
//		}
		
		// still exists, delete it the old way		
		Bacon_GunBuilderUI_InvCb_UIResponse cb = new Bacon_GunBuilderUI_InvCb_UIResponse();
			
		cb.messageOk = "Item removed";
		cb.messageFailed = "Failed to remove item from storage";
		cb.request = request;
		cb.component = this;

		storageManager.TryDeleteItem(attachedEntity, cb);
	}
	
	void Action_ReplaceItemInSlotWithPrefab(Bacon_GunBuilderUI_Network_StorageRequest request, BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent storageManager) {
		InventoryStorageSlot slot = storage.GetSlot(request.storageSlotId);
		if (!slot) {
			SendActionResponse(request, false, "Requested slot does not exist");
			return;
		}
		
		if (!HasHighEnoughRank(request))
			return;

		float cost;
		if (!CanAffordItem(request, cost))
			return;
			
		string messageOk = "Item added";
		if (cost > 0) {
			messageOk = string.Format("%1 (-%2 supply)", messageOk, cost);
		}
		
		// try refunding current attached entity
		IEntity attachedEntity = slot.GetAttachedEntity();

		float refund = 0;
		if (attachedEntity) {
			if (!TryRefundItem(request, attachedEntity, refund))
				return;
		}
		if (refund > 0) {
			messageOk = string.Format("%1 (+%2 refund)", messageOk, refund);
		}
		
  		if (!attachedEntity) {			
			// IEntity itemEntity = Bacon_GunBuilderUI_Helpers.PrepareTemporaryEntity(request.prefab);
			IEntity itemEntity = Bacon_GunBuilderUI_Helpers.PrepareTemporaryEntityAtCoords(request.prefab, storageManager.GetOwner().GetOrigin());
			if (!itemEntity) {
				SendActionResponse(request, false, "Failed to spawn temporary entity"); return; }

			Bacon_GunBuilderUI_InvCb_DeleteTemporaryEntityOnFailure deleteCb = new Bacon_GunBuilderUI_InvCb_DeleteTemporaryEntityOnFailure();
			
			deleteCb.messageOk = messageOk;
			deleteCb.messageFailed = "Failed to add prefab into substorage";
			deleteCb.temporaryEntity = itemEntity;
			deleteCb.component = this;
			deleteCb.request = request;

			storageManager.TryInsertItemInStorage(itemEntity, storage, request.storageSlotId, deleteCb);
			
//			cb.messageOk = "Slot updated";
//			cb.messageFailed = "Failed to spawn prefab into storage";
//			cb.component = this;
//			cb.request = request;
//			
//			storageManager.TrySpawnPrefabToStorage(request.prefab, storage, request.storageSlotId, EStoragePurpose.PURPOSE_ANY, cb);
		} else {
			// refund failed - entity still exists
			Bacon_GunBuilder_InvCb_SpawnAfterDelete deleteCb = new Bacon_GunBuilder_InvCb_SpawnAfterDelete();

			deleteCb.request = request;
			deleteCb.component = this;
			deleteCb.storageManager = storageManager;
			deleteCb.slotStorage = storage;
	
			storageManager.TryDeleteItem(attachedEntity, deleteCb);
		}
	}
	
	void SendActionResponse(Bacon_GunBuilderUI_Network_Request request, bool success, string message = "") {
		Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RpcDo_SendActionResponse | Operation: %1, success: %2, message: %3", request.Repr(), success, message), LogLevel.DEBUG);
		
		Bacon_GunBuilderUI_Network_Response response = new Bacon_GunBuilderUI_Network_Response();
		response.request = request;
		response.success = success;
		response.message = message;
		// response.Pack();
		
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", response);
		
		string responseString = saveContext.ExportToString();
		
		Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.SendActionResponse | Sending response: %1", responseString), LogLevel.DEBUG);
		
		Rpc(RpcDo_SendActionResponse, responseString);
	}
	
	// ---------- loadout stuff
	// loadout request
	void RequestLoadoutAction(Bacon_GunBuilderUI_Network_LoadoutRequest request) {
		SCR_JsonSaveContext saveContext = new SCR_JsonSaveContext();
		saveContext.WriteValue("", request);

		string requestString = saveContext.ExportToString();
		
		Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RequestLoadoutAction | Sending request: %1", requestString), LogLevel.DEBUG);
		Rpc(RpcAsk_RequestLoadoutAction, requestString);
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestLoadoutAction(string requestJson) {
		int playerId = m_PC.GetPlayerId();

		Bacon_GunBuilderUI_Network_LoadoutRequest request = new Bacon_GunBuilderUI_Network_LoadoutRequest();
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(requestJson);
		loadContext.ReadValue("", request);
		
		if (!m_LoadoutStorageComponent) {
			SendActionResponse(request, false, "Loadout manager component missing from game mode");
			return;
		}

		SCR_ArsenalManagerComponent arsenalManager;
		if (!SCR_ArsenalManagerComponent.GetArsenalManager(arsenalManager)) {
			SendActionResponse(request, false, "No Arsenal Manager found in the world");
			return;
		}
		
		Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RpcAsk_RequestLoadoutAction | Processing request from player %1: %2", playerId, requestJson), LogLevel.DEBUG);
		
//		BackendApi backend = GetGame().GetBackendApi();
//		string identity = backend.GetPlayerIdentityId(playerId);
//		
//		if (identity.IsEmpty()) {
//			if (GetGame().IsDev()) {
//				Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RpcAsk_RequestLoadoutAction | Setting dev identity for playerid %1", playerId), LogLevel.DEBUG);
//				identity = "DEV_IDENTITY";
//			} else {
//				Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RpcAsk_RequestLoadoutAction | No identity id for player %1: %2", playerId, requestJson), LogLevel.ERROR);
//				return;
//			}
//		}
		string identity;
		if (!Bacon_GunBuilderUI_Helpers.GetPlayerIdentityId(playerId, identity)) {
			SendActionResponse(request, false, "Invalid player identity id");
			return;
		}
		
		string factionKey;
		if (!Bacon_GunBuilderUI_Helpers.GetPlayerEntityFactionKey(playerId, factionKey)) {
			SendActionResponse(request, false, "Invalid faction");
			return;
		}
		
		Managed entity = Replication.FindItem(request.arsenalComponentRplId);
		if (!entity) {
			SendActionResponse(request, false, "Invalid Arsenal Component provided");
			return;
		}
		
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(entity);
		if (!entity) {
			SendActionResponse(request, false, "RplId is not an Arsenal Component");
			return;
		}

		switch (request.actionType) {
			case Bacon_GunBuilderUI_ActionType.GET_LOADOUTS: {
				Action_GetLoadoutList(request, identity, factionKey, playerId);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.SAVE_LOADOUT: {
				Action_SavePlayerLoadout(request, arsenalManager, arsenal, identity, factionKey, playerId);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.APPLY_LOADOUT: {
				Action_ApplyPlayerLoadout(arsenal, arsenalManager, request, identity, factionKey, playerId);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.CLEAR_LOADOUT: {
				Action_ClearPlayerLoadout(request, identity, factionKey, playerId);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.GET_ADMIN_LOADOUTS: {
				//if (!SCR_Global.IsAdmin(m_PC.GetPlayerId())) {
				//	SendActionResponse(request, false, "Not admin");
				//	return;
				//}
				Action_GetLoadoutList(request, identity, factionKey, playerId, true);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.SAVE_LOADOUT_ADMIN: {
				if (!SCR_Global.IsAdmin(m_PC.GetPlayerId())) {
					SendActionResponse(request, false, "Not admin");
					return;
				}
				Action_SavePlayerLoadout(request, arsenalManager, arsenal, identity, factionKey, playerId, true);
				
				if (!Bacon_GunBuilderUI_PlayerControllerComponent.ServerInstance) {
					PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent | Server instance of player controller component not found! Cannot send loadouts.", LogLevel.ERROR);
					return;
				}
				
				Bacon_GunBuilderUI_PlayerControllerComponent.ServerInstance.UpdateServerLoadouts();
				Bacon_GunBuilderUI_PlayerControllerComponent.ServerInstance.BroadcastLoadoutChange();
				break;
			}
			case Bacon_GunBuilderUI_ActionType.APPLY_LOADOUT_ADMIN: {
				//if (!SCR_Global.IsAdmin(m_PC.GetPlayerId())) {
				//	SendActionResponse(request, false, "Not admin");
				//	return;
				//}
				Action_ApplyPlayerLoadout(arsenal, arsenalManager, request, identity, factionKey, playerId, true);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.CLEAR_LOADOUT_ADMIN: {
				if (!SCR_Global.IsAdmin(m_PC.GetPlayerId())) {
					SendActionResponse(request, false, "Not admin");
					return;
				}
				Action_ClearPlayerLoadout(request, identity, factionKey, playerId, true);
				break;
			}
		}
	}

	void SetAILoadout(int slotId, IEntity target, SCR_AIGroup group) {
		if (!target || slotId < 0)
			return;
		
		if (slotId > AdminLoadoutMetadata.Count())
			return;

		PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout | Loading data for GM slot %1", slotId, level: LogLevel.DEBUG);
		
		string loadoutData;
		string prefab;
		float cost;
		
		int slotIdInternal = AdminLoadoutMetadata[slotId].slotId;
		
		if (!m_LoadoutStorageComponent.GetPlayerLoadoutData(0, "", slotIdInternal, prefab, loadoutData, cost, true)) {
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout | Cannot load loadout data for slot %1", slotIdInternal, level: LogLevel.ERROR);
			return;
		}
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;

		target.GetWorldTransform(params.Transform);
		params.Transform[3] = params.Transform[3] + (vector.Up*0.05);
		
		if (prefab.IsEmpty()) {
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout | Invalid character prefab", level: LogLevel.ERROR);
			return;
		}
		
		Resource loaded = Resource.Load(prefab);
		if (!loaded) {
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout | Failed to load character prefab", level: LogLevel.ERROR);
			return;
		}
		
		IEntity entity = GameEntity.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, GetGame().GetWorld(), params));
		if (!entity) {
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout | Spawning character entity failed", level: LogLevel.ERROR);
			return;
		}
		
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if (!controller) {
			RplComponent.DeleteRplEntity(entity, false);
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout | Spawned entity has no Character Controller Component", level: LogLevel.ERROR);
			return;
		}
		
		controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);
		
		GetGame().GetCallqueue().CallLater(SetAILoadout_StepTwo, 100, false, loadoutData, controller, entity, target, group);
	}
	void SetAILoadout_StepTwo(string loadoutData, CharacterControllerComponent controller, IEntity newEntity, IEntity previousEntity, SCR_AIGroup group) {
		// controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);

		if (!newEntity || !previousEntity || !controller) {
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout_StepTwo | An important entity disappeared before the loadout could be loaded for AI", level: LogLevel.ERROR);
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout_StepTwo | newEntity: %1", newEntity, level: LogLevel.ERROR);
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout_StepTwo | previousEntity: %1", previousEntity, level: LogLevel.ERROR);
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.SetAILoadout_StepTwo | controller: %1", controller, level: LogLevel.ERROR);
			return;
		}

		GameEntity entityGame = GameEntity.Cast(newEntity);
		
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		ctx.ImportFromString(loadoutData);
		// ctx.ReadValue("", entityGame);
		SCR_PlayerArsenalLoadout.ApplyLoadoutString(entityGame, ctx);

		group.AddAgentFromControlledEntity(newEntity);
		AIControlComponent aiControlComponent = AIControlComponent.Cast(newEntity.FindComponent(AIControlComponent));
		aiControlComponent.ActivateAI();
		
		RplComponent.DeleteRplEntity(previousEntity, false);
	}
	
	bool HasEnoughRankForLoadout(IEntity contolledEntity, string requiredRankStr, SCR_ArsenalManagerComponent arsenalManager) {
		if (!arsenalManager.AreItemsRankLocked())
			return true;
		
		SCR_ECharacterRank requiredRank = typename.StringToEnum(SCR_ECharacterRank, requiredRankStr);
		if (requiredRank == SCR_ECharacterRank.INVALID || requiredRank == SCR_ECharacterRank.RENEGADE)
			return true;
		
		SCR_ECharacterRank playerRank = SCR_CharacterRankComponent.GetCharacterRank(contolledEntity);
		if (playerRank == SCR_ECharacterRank.INVALID)
			return true;
		
		return playerRank >= requiredRank;
	}
	
	void Action_ApplyPlayerLoadout(SCR_ArsenalComponent arsenal, SCR_ArsenalManagerComponent arsenalManager, Bacon_GunBuilderUI_Network_LoadoutRequest request, string identity, string factionKey, int playerId, bool isAdminLoadout = false) {
		IEntity previousEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!previousEntity)
			return;
		
		string loadoutData;
		string prefab;
		string requiredRank;
		float cost;
		
		if (!m_LoadoutStorageComponent.GetPlayerLoadoutData(playerId, factionKey, request.loadoutSlotId, prefab, loadoutData, cost, isAdminLoadout, requiredRank)) {
			SendActionResponse(request, false, "Failed to load loadout");
			return;
		}
		
		// might this work?
		string templatePrefab = Bacon_GunBuilderUI_Helpers.GetArsenalLoadoutTemplateForFaction(factionKey);
		if (templatePrefab.IsEmpty()) {
			SendActionResponse(request, false, "Arsenal loadout template is invalid - misconfigured Loadout Manager?");
			return;
		}
		
		prefab = templatePrefab;
		
		if (!HasEnoughRankForLoadout(previousEntity, requiredRank, arsenalManager)) {
			SendActionResponse(request, false, string.Format("Rank too low (required: %1)", requiredRank));
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;

		previousEntity.GetWorldTransform(params.Transform);
		params.Transform[3] = params.Transform[3] + (vector.Up*0.05);
		
		if (prefab.IsEmpty()) {
			SendActionResponse(request, false, "Invalid character prefab");
			return;
		}
		
		Resource loaded = Resource.Load(prefab);
		if (!loaded) {
			SendActionResponse(request, false, "Failed to load character prefab");
			return;
		}
		
		// cost calculation here
		if (!isAdminLoadout && cost > 0) {
			if (!CanAffordLoadout(arsenal, request, cost))
				return;
		}
		
		IEntity entity = GameEntity.Cast(GetGame().SpawnEntityPrefabEx(prefab, false, GetGame().GetWorld(), params));
		if (!entity) {
			SendActionResponse(request, false, "Spawning character entity failed");
			TryRefundFixedCost(arsenal.GetOwner(), cost);
			return;
		}
		
		// will this magically work? @bacon
		SCR_ChimeraAIAgent agent = Bacon_GunBuilderUI_Helpers.FindAIAgent(entity);
		if (agent)
			agent.SetPlayerPending_S(playerId);
		
		SCR_EditableCharacterComponent editorCharacter = SCR_EditableCharacterComponent.Cast(entity.FindComponent(SCR_EditableCharacterComponent));
		if (editorCharacter)
			editorCharacter.SetIsPlayerPending(playerId);
		
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if (!controller) {
			RplComponent.DeleteRplEntity(entity, false);
			SendActionResponse(request, false, "Spawned entity has no Character Controller Component");
			return;
		}
		
		controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);
		
		// compute cost of current loadout
		string serialized;
		float currentLoadoutCost = 0;
		if (Bacon_GunBuilder_PlayerFactionLoadoutStorage.SerializeCharacter(previousEntity, serialized)) {
			if (!arsenalManager.GunBuilderUI_GetLoadoutRespawnCost(serialized, Bacon_GunBuilderUI_Helpers.GetFactionFromFactionKey(factionKey), currentLoadoutCost))
				PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.Action_ApplyPlayerLoadout | Failed to get cost of current loadout, refund will not be given...", level: LogLevel.WARNING);
		}
		
		GetGame().GetCallqueue().CallLater(Action_ApplyPlayerLoadout_StepTwo, 200, false, arsenal.GetOwner(), loadoutData, controller, entity, previousEntity, request, playerId, cost, currentLoadoutCost);
		// Action_ApplyPlayerLoadout_StepTwo(loadoutData, entity, previousEntity, request, playerId);
	}
	void Action_ApplyPlayerLoadout_StepTwo(IEntity arsenal, string loadoutData, CharacterControllerComponent controller, IEntity newEntity, IEntity previousEntity, Bacon_GunBuilderUI_Network_LoadoutRequest request, int playerId, float refundCost, float currentLoadoutCost) {
		// controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedDeliberate, true);
		string identity;
		Bacon_GunBuilderUI_Helpers.GetPlayerIdentityId(playerId, identity);
		PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.Action_ApplyPlayerLoadout_StepTwo | Loading loadout for player id: %1, identity: %2", playerId, identity, level: LogLevel.NORMAL);
		
		if (!newEntity || !previousEntity || !controller) {
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.Action_ApplyPlayerLoadout_StepTwo | An important entity disappeared before the loadout could be loaded for player id: %1, identity: %2", playerId, identity, level: LogLevel.ERROR);
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.Action_ApplyPlayerLoadout_StepTwo | newEntity: %1", newEntity, level: LogLevel.ERROR);
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.Action_ApplyPlayerLoadout_StepTwo | previousEntity: %1", previousEntity, level: LogLevel.ERROR);
			PrintFormat("Bacon_GunBuilderUI_PlayerControllerComponent.Action_ApplyPlayerLoadout_StepTwo | controller: %1", controller, level: LogLevel.ERROR);
			TryRefundFixedCost(arsenal, refundCost);
			return;
		}

		GameEntity entityGame = GameEntity.Cast(newEntity);

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		ctx.ImportFromString(loadoutData);
		
		SCR_PlayerArsenalLoadout.ApplyLoadoutString(entityGame, ctx);
		// ctx.ReadValue("", entityGame);
		
		SCR_ECharacterRank rank = SCR_CharacterRankComponent.GetCharacterRank(previousEntity);
		if (rank != SCR_ECharacterRank.INVALID) {
			SCR_CharacterRankComponent comp = SCR_CharacterRankComponent.Cast(newEntity.FindComponent(SCR_CharacterRankComponent));
			if (comp)
				comp.SetCharacterRank(rank, true);
		}

		SCR_PlayerController.Cast(GetOwner()).SetInitialMainEntity(newEntity);
		
		string message = "Loadout applied";
		if (currentLoadoutCost > 0 && Bacon_GunBuilderUI_Helpers.AreSuppliesEnabled(arsenal)) {
			message = string.Format("%1 (supply refund: %2)", message, currentLoadoutCost);
			TryRefundFixedCost(arsenal, currentLoadoutCost);
		}
		
		SendActionResponse(request, true, message);
		RplComponent.DeleteRplEntity(previousEntity, false);
		AfterLoadoutAppliedSuccessfully(playerId, newEntity);
	}
	protected void AfterLoadoutAppliedSuccessfully(int playerId, IEntity newEntity) {
		if (m_MapMarkerEntrySquadLeader)
			m_MapMarkerEntrySquadLeader.BaconLoadoutEditor_UpdateMarkerTarget(playerId);
		else
			Print("Bacon_GunBuilderUI_PlayerControllerComponent.AfterLoadoutAppliedSuccessfully | m_MapMarkerEntrySquadLeader is null - cannot update marker", LogLevel.WARNING);

		SCR_PerceivedFactionManagerComponent perceivedFactionManager = SCR_PerceivedFactionManagerComponent.GetInstance();
		if (perceivedFactionManager)
			perceivedFactionManager.OnPlayerSpawnFinalize_S(null, null, null, newEntity);
		else
			Print("Bacon_GunBuilderUI_PlayerControllerComponent.AfterLoadoutAppliedSuccessfully | Could not find SCR_PerceivedFactionManagerComponent", LogLevel.WARNING);

		if (m_gameMode)
			m_gameMode.GetOnPlayerSpawned().Invoke(playerId, newEntity);
		else
			Print("Bacon_GunBuilderUI_PlayerControllerComponent.AfterLoadoutAppliedSuccessfully | No game mode!", LogLevel.ERROR);
	}
	
	void Action_GetLoadoutList(Bacon_GunBuilderUI_Network_LoadoutRequest request, string identity, string factionKey, int playerId, bool isAdminLoadout = false) {
		array<ref Bacon_GunBuilder_PlayerLoadout> loadoutOptions = {};

		// Bacon_GunBuilder_LoadoutStorage.GetPlayerLoadoutMetadata(identity, factionKey, loadoutOptions);
		m_LoadoutStorageComponent.GetPlayerLoadoutMetadata(playerId, identity, factionKey, loadoutOptions, isAdminLoadout);

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("loadouts", loadoutOptions);
		
		SendActionResponse(request, true, ctx.ExportToString());
	}
	
	void Action_SavePlayerLoadout(Bacon_GunBuilderUI_Network_LoadoutRequest request, SCR_ArsenalManagerComponent arsenalManager, SCR_ArsenalComponent arsenal, string identity, string factionKey, int playerId, bool isAdminLoadout = false) {
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!controlledEntity)
			return;

		if (!isAdminLoadout && !arsenalManager.GunBuilderUI_CanSaveLoadout(playerId, GameEntity.Cast(controlledEntity), FactionAffiliationComponent.Cast(controlledEntity.FindComponent(FactionAffiliationComponent)), arsenal, true)) {
			SendActionResponse(request, false, "Cannot save loadout: Rejected by Arsenal properties (invalid faction items?)");
			return;
		}
		
		if (!m_LoadoutStorageComponent.SaveCurrentPlayerLoadout(arsenalManager, playerId, identity, controlledEntity, factionKey, request.loadoutSlotId, isAdminLoadout)) {
			SendActionResponse(request, false, "Loadout saving failed");
			return;
		}
		
		
		
		Action_GetLoadoutList(request, identity, factionKey, playerId, isAdminLoadout);
	}
	void Action_ClearPlayerLoadout(Bacon_GunBuilderUI_Network_LoadoutRequest request, string identity, string factionKey, int playerId, bool isAdminLoadout = false) {
		if (!m_LoadoutStorageComponent.ClearLoadoutSlot(playerId, identity, factionKey, request.loadoutSlotId, isAdminLoadout)) {
			SendActionResponse(request, false, "Failed to clear loadout slot");
			return;
		}
		
		Action_GetLoadoutList(request, identity, factionKey, playerId, isAdminLoadout);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_SendActionResponse(string responseJson) {
		Print(string.Format("Bacon_GunBuilderUI_PlayerControllerComponent.RpcDo_SendActionResponse | Processing response: %1", responseJson), LogLevel.DEBUG);
		
		// Print(response.request);
		SCR_JsonLoadContext loadContext = new SCR_JsonLoadContext();
		loadContext.ImportFromString(responseJson);
		
		Bacon_GunBuilderUI_Network_Response response = new Bacon_GunBuilderUI_Network_Response();
		loadContext.ReadValue("", response);

		switch (response.request.actionType) {
			case Bacon_GunBuilderUI_ActionType.ADD_ITEM:
			case Bacon_GunBuilderUI_ActionType.REMOVE_ITEM:
			case Bacon_GunBuilderUI_ActionType.REPLACE_ITEM: 
			case Bacon_GunBuilderUI_ActionType.CHANGE_VISUAL_IDENTITY: 
			case Bacon_GunBuilderUI_ActionType.CHANGE_SOUND_IDENTITY: {
				Bacon_GunBuilderUI_Network_StorageRequest storageRequest;
				loadContext.ReadValue("request", storageRequest);
				
				m_OnResponse_Storage.Invoke(response, storageRequest);
				break;
			}
			case Bacon_GunBuilderUI_ActionType.GET_ADMIN_LOADOUTS:
			case Bacon_GunBuilderUI_ActionType.SAVE_LOADOUT_ADMIN:
			case Bacon_GunBuilderUI_ActionType.APPLY_LOADOUT_ADMIN:
			case Bacon_GunBuilderUI_ActionType.CLEAR_LOADOUT_ADMIN:
			case Bacon_GunBuilderUI_ActionType.GET_LOADOUTS:
			case Bacon_GunBuilderUI_ActionType.SAVE_LOADOUT:
			case Bacon_GunBuilderUI_ActionType.CLEAR_LOADOUT:
			case Bacon_GunBuilderUI_ActionType.APPLY_LOADOUT: {
				Bacon_GunBuilderUI_Network_LoadoutRequest loadoutRequest;
				loadContext.ReadValue("request", loadoutRequest);

				m_OnResponse_Loadout.Invoke(response, loadoutRequest);
				break;
			}
		}
		
		
	};

};

class Bacon_GunBuilderUI_InvCb: ScriptedInventoryOperationCallback {
	Bacon_GunBuilderUI_Network_Request request;
	string messageOk;
	string messageFailed;
	
	Bacon_GunBuilderUI_PlayerControllerComponent component;
//	RplId storageRplId;
//	int slotId;

	Bacon_GunBuilderUI_InvCb nextCb;
	
	override void OnComplete() {
		if (component)
			component.SendActionResponse(request, true, messageOk);
	};
	override void OnFailed() {
		if (component)
			component.SendActionResponse(request, false, messageFailed);
	};	
}

sealed class Bacon_GunBuilderUI_InvCb_UIResponse: Bacon_GunBuilderUI_InvCb {
	static Bacon_GunBuilderUI_InvCb_UIResponse CreateResponseCallback(Bacon_GunBuilderUI_InvCb fromCb) {
		Bacon_GunBuilderUI_InvCb_UIResponse cb = new Bacon_GunBuilderUI_InvCb_UIResponse();

		cb.request = fromCb.request;
		cb.component = fromCb.component;
		
		return cb;
	}
};

sealed class Bacon_GunBuilderUI_InvCb_DeleteTemporaryEntityOnFailure: Bacon_GunBuilderUI_InvCb {
	IEntity temporaryEntity;
	
	override void OnFailed() {
		SCR_EntityHelper.DeleteEntityAndChildren(temporaryEntity);

		super.OnFailed();
	};
}

sealed class Bacon_GunBuilder_InvCb_SpawnAfterDelete: Bacon_GunBuilderUI_InvCb {
	SCR_InventoryStorageManagerComponent storageManager;
	BaseInventoryStorageComponent slotStorage;
	
	override void OnComplete() {
		Bacon_GunBuilderUI_Network_StorageRequest storageRequest = Bacon_GunBuilderUI_Network_StorageRequest.Cast(request);
		
		if (storageRequest.prefab != "empty") {
			// IEntity itemEntity = Bacon_GunBuilderUI_Helpers.PrepareTemporaryEntity(storageRequest.prefab);
			IEntity itemEntity = Bacon_GunBuilderUI_Helpers.PrepareTemporaryEntityAtCoords(storageRequest.prefab, storageManager.GetOwner().GetOrigin());
			if (!itemEntity) {
				messageFailed = string.Format("Failed to create temporary entity from prefab: %1", storageRequest.prefab);
				OnFailed(); return;
			}

			Bacon_GunBuilderUI_InvCb_DeleteTemporaryEntityOnFailure deleteCb = new Bacon_GunBuilderUI_InvCb_DeleteTemporaryEntityOnFailure();
				
			deleteCb.messageOk = "Item added";
			deleteCb.messageFailed = "Failed to add prefab into substorage";
			deleteCb.temporaryEntity = itemEntity;
			deleteCb.component = component;
			deleteCb.request = request;
	
			storageManager.TryInsertItemInStorage(itemEntity, slotStorage, storageRequest.storageSlotId, deleteCb);
			return;

//			Bacon_GunBuilderUI_InvCb_UIResponse responseCb = Bacon_GunBuilderUI_InvCb_UIResponse.CreateResponseCallback(this);
//			responseCb.messageOk = "Slot updated";
//			responseCb.messageFailed = string.Format("Failed to spawn prefab into storage %1 - prefab %2", slotStorage.Type(), storageRequest.prefab);
//			
//			storageManager.TrySpawnPrefabToStorage(storageRequest.prefab, slotStorage, storageRequest.storageSlotId, EStoragePurpose.PURPOSE_ANY, responseCb);
//			return;
		}
		
		messageOk = "Item removed";
		super.OnComplete();
	};
	override void OnFailed() {
		if (messageFailed.IsEmpty()) {
			Bacon_GunBuilderUI_Network_StorageRequest storageRequest = Bacon_GunBuilderUI_Network_StorageRequest.Cast(request);
			messageFailed = string.Format("Failed to delete item %1 in storage %2", storageRequest.storageSlotId, slotStorage.Type());
		}
		
		super.OnFailed();
	};
}
