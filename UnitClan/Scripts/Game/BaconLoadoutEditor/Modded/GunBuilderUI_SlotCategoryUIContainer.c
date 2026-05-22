modded class GunBuilderUI_CategoryButtonsUIComponent {
	override void InitCategoryButtons() {
		int count;

		array<string> enumNames = {};
		if (m_eCategoryMode == Bacon_GunBuilderUI_CategoryUIComponent_Mode.CHARACTER) {
			count = SCR_Enum.GetEnumNames(Bacon_GunBuilderUI_SlotCategory, enumNames);
		} else {
			count = SCR_Enum.GetEnumNames(Bacon_GunBuilderUI_ArsenalCategory, enumNames);
		}
		
		ResourceName imageset;
		string iconName;
		string label;
		
		for (int i = 0; i < count; i++) {
			//if (enumNames[i] == "SERVER_LOADOUTS" && !m_bIsAdmin) {
				//continue;
			//}
			
			Widget button = Widget.Cast(GetGame().GetWorkspace().CreateWidgets(m_sResourceName_CategoryButton, m_layoutMain));
			GunBuilderUI_CategoryButtonUIComponent handler = GunBuilderUI_CategoryButtonUIComponent.Cast(button.FindHandler(GunBuilderUI_CategoryButtonUIComponent));
			
			handler.m_OnClicked.Insert(OnButtonClicked);
			handler.SetCategoryName(enumNames[i]);
			
			GetCategoryIcon(enumNames[i], imageset, iconName);
			handler.SetImageFromImageSet(imageset, iconName);
			
			if (m_eCategoryMode == Bacon_GunBuilderUI_CategoryUIComponent_Mode.CHARACTER) {
				handler.SetLabel(GetCategoryLabelCharacter(enumNames[i]));

//				if (enumNames[i] == "LOADOUTS" && !SCR_Global.IsEditMode())
//					handler.SetEnabled(false);
			} else {
				handler.SetLabel(GetCategoryLabelArsenal(enumNames[i]));
			}
			
			m_wButtonComponents.Insert(handler);
		}
	}
}