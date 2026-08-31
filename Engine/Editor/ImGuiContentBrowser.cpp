#include "ImGuiContentBrowser.h"
#include "TUFEngine.h"


static const std::filesystem::path s_AssetsPath = "resources";

ImGuiContentBrowser::ImGuiContentBrowser()
	:m_CurrentDirectory(s_AssetsPath)
{

}

bool ImGuiContentBrowser::isImageFile(const std::filesystem::path& filePath) const
{
	std::string ext = filePath.extension().string();
	for (auto& c : ext) c = (char)tolower(c);
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

//あとで変更予定

void ImGuiContentBrowser::update(TUFEngine* engine) {
#ifdef USE_IMGUI

	if (m_DirectoryIcon == -1) {
		//フォルダの画像とか読み込み
		m_DirectoryIcon = TextureManager::GetInstance()->LoadTexture("ContentBrowserAsset/Icons/ContentBrowser/DirectoryIcon.png");
		m_FileIcon = TextureManager::GetInstance()->LoadTexture("ContentBrowserAsset/Icons/ContentBrowser/FileIcon2.png");
	}

	if (!show) return;

	if (begin("コンテンツブラウザ")) {

		if (m_CurrentDirectory != std::filesystem::path(s_AssetsPath))
		{
			if (ImGui::Button("←戻る")) {
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}

		}

		static float padding = 16.0f;
		static float thumbnailSize = 60;
		float cellSize = thumbnailSize + padding;

		float panelWindth = ImGui::GetContentRegionAvail().x;
		int colummCount = (int)(panelWindth / cellSize);
		if (colummCount < 1)
			colummCount = 1;
		ImGui::Columns(colummCount, 0, false);


		for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory))
		{
			const auto& path = directoryEntry.path();
			auto relativePath = std::filesystem::relative(path, s_AssetsPath);
			std::string filenameString = relativePath.filename().string();

			ImGui::PushID(filenameString.c_str());

			int textureIndex;
			if (directoryEntry.is_directory())
			{
				textureIndex = m_DirectoryIcon;
			}
			else if (isImageFile(path))
			{
				auto it = m_ThumbnailCache.find(filenameString);
				if (it != m_ThumbnailCache.end())
				{
					textureIndex = it->second;
				}
				else
				{
					textureIndex = TextureManager::GetInstance()->LoadTexture(path.string());
					m_ThumbnailCache[filenameString] = textureIndex;
				}
			}
			else
			{
				textureIndex = m_FileIcon;
			}

			auto gpuHandle = TextureManager::GetInstance()->GetGPUHandle(textureIndex);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
			ImGui::ImageButton(filenameString.c_str(), (ImTextureID)gpuHandle.ptr, { thumbnailSize,thumbnailSize });


			if (ImGui::BeginDragDropSource()) 
			{
				const wchar_t* itemPath = relativePath.c_str();
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM",itemPath,(wcslen(itemPath)+1)*sizeof(wchar_t));				
				ImGui::Text("%s", filenameString.c_str());

				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{

				if (directoryEntry.is_directory())
					m_CurrentDirectory /= path.filename();
				if (!directoryEntry.is_directory())
				{
					// engineを経由してUIManagerのコールバックを呼ぶ
					auto manager = engine->GetImGuiManager();
					if (manager->onFileDrop)
					{
						manager->onFileDrop(relativePath.wstring());
					}
				}
			}


			

			ImGui::TextWrapped(filenameString.c_str());

			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);

		ImGui::SliderFloat("サムネイルサイズ", &thumbnailSize, 16, 512);
		ImGui::SliderFloat("パディング", &padding, 0, 32);

	}
	end();

#endif
}