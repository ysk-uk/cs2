#include "imgui.h"
#include "gui.h"

void draw_Menu() {
	ImGui::Begin("ZeroFlick"); {
		ImGui::Checkbox("box esp", &zeroflick::visuals::box);
		ImGui::Checkbox("trigger", &zeroflick::visuals::trigger);
		ImGui::Checkbox("Bone_Start", &zeroflick::visuals::Bone_Start);
		ImGui::Checkbox("Aim_Start", &zeroflick::visuals::Aim_Start);

		}
	ImGui::End();
}
