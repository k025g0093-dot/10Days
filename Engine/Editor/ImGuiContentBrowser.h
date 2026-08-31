#pragma once
#include "ImGuiWindow.h"
#include <filesystem>
#include <string>
#include <unordered_map>

class ImGuiContentBrowser : public ImGuiUIWindow {
public:
    ImGuiContentBrowser();
    void update(TUFEngine* engine) override;
private:
    bool isImageFile(const std::filesystem::path& filePath) const;

    std::filesystem::path m_CurrentDirectory;
    int m_DirectoryIcon = -1;
    int m_FileIcon = -1;
    std::unordered_map<std::string, int> m_ThumbnailCache;
};