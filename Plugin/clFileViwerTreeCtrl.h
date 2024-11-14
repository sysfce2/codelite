//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// Copyright            : (C) 2015 Eran Ifrah
// File name            : clFileViwerTreeCtrl.h
//
// -------------------------------------------------------------------------
// A
//              _____           _      _     _ _
//             /  __ \         | |    | |   (_) |
//             | /  \/ ___   __| | ___| |    _| |_ ___
//             | |    / _ \ / _  |/ _ \ |   | | __/ _ )
//             | \__/\ (_) | (_| |  __/ |___| | ||  __/
//              \____/\___/ \__,_|\___\_____/_|\__\___|
//
//                                                  F i l e
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#ifndef CLFILEVIWERTREECTRL_H
#define CLFILEVIWERTREECTRL_H

#include "clThemedTreeCtrl.h"
#include "codelite_exports.h"
#include "wxDataViewTreeCtrl.hpp"
#include "wxStringHash.h"

#include <map>
#include <wx/dataview.h>
#include <wx/filename.h>
#include <wx/treectrl.h>

/**
 * @class clTreeNodeIndex
 * @brief a helper class for fast searching a folder in the tree view
 */
class WXDLLIMPEXP_SDK clTreeNodeIndex
{
    std::unordered_map<wxString, wxTreeItemId> m_children;

public:
    clTreeNodeIndex() {}
    virtual ~clTreeNodeIndex() {}

    wxTreeItemId Find(const wxString& path);
    void Add(const wxString& path, const wxTreeItemId& item);
    void Delete(const wxString& name);

    /**
     * @brief clear the index content
     */
    void Clear() { m_children.clear(); }
};

// Item data class
class WXDLLIMPEXP_SDK clTreeCtrlData : public wxTreeItemData
{
public:
    enum eKind {
        kDummy,  // Dummy node item
        kRoot,   // This item is the root (non visible) item
        kFile,   // a file
        kFolder, // a folder
    };
    eKind m_kind;
    // If file, contains the fullpath otherwise, contains the path
    wxString m_path;
    wxString m_name;
    clTreeNodeIndex* m_index;

public:
    clTreeCtrlData(eKind kind)
        : m_kind(kind)
        , m_index(NULL)
    {
        if (IsFolder()) {
            m_index = new clTreeNodeIndex();
        }
    }
    virtual ~clTreeCtrlData() { wxDELETE(m_index); }

    clTreeNodeIndex* GetIndex() { return m_index; }

    void SetKind(const eKind& kind) { this->m_kind = kind; }
    const eKind& GetKind() const { return m_kind; }

    /**
     * @brief set the path and update the display name
     */
    void SetPath(const wxString& path)
    {
        this->m_path = path;
        if (IsFolder()) {
            wxFileName fn(m_path, "");
            if (fn.GetDirCount()) {
                m_name = fn.GetDirs().Last();
            } else {
                m_name = m_path;
            }
        } else if (IsFile()) {
            wxFileName fn(m_path);
            m_name = fn.GetFullName();
        } else {
            m_name = wxEmptyString;
        }
    }

    /**
     * @brief return the item's path. if it is a folder, return the folder path. If it is a file
     * return the file's fullpath
     */
    const wxString& GetPath() const { return m_path; }

    /**
     * @brief return the display name
     */
    const wxString& GetName() const { return m_name; }

    // Aliases
    bool IsFolder() const { return m_kind == kFolder; }
    bool IsFile() const { return m_kind == kFile; }
    bool IsDummy() const { return m_kind == kDummy; }
};

#include "bitmap_loader.h"
class WXDLLIMPEXP_SDK clFileViewerTreeCtrl : public wxTreeCtrlDataViewBase
{
    wxDECLARE_DYNAMIC_CLASS(clFileViewerTreeCtrl);

public:
    clFileViewerTreeCtrl();
    clFileViewerTreeCtrl(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
                         const wxSize& size = wxDefaultSize,
                         long style = wxTR_DEFAULT_STYLE | wxTR_MULTIPLE | wxTR_HIDE_ROOT | wxBORDER_NONE);
    virtual ~clFileViewerTreeCtrl();

    // void SetBitmaps(BitmapLoader::Vec_t* bitmaps);

private:
    BitmapLoader::Vec_t* m_bitmaps = nullptr;
};

#endif // CLFILEVIWERTREECTRL_H
