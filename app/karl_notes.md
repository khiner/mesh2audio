Example of using `ImGui::Image` with `ImDrawList::AddCallback`:
https://github.com/ocornut/imgui/issues/5485#issuecomment-1190146953

Should prob try without `AddCallback` at first (just render to a texture and use `AddImage`), but callback might be helpful/needed for dimension/projection matching.
