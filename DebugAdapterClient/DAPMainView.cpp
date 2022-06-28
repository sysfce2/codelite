#include "DAPMainView.h"

#include "globals.h"
#include "macros.h"

struct VariableClientData : public wxTreeItemData {
    int reference = wxNOT_FOUND;
    VariableClientData(int ref)
        : reference(ref)
    {
    }
    virtual ~VariableClientData() {}
};

DAPMainView::DAPMainView(wxWindow* parent, dap::Client* client, clModuleLogger& log)
    : DAPMainViewBase(parent)
    , m_client(client)
    , LOG(log)
{
    m_threadsTree->SetTreeStyle(m_threadsTree->GetTreeStyle() | wxTR_HIDE_ROOT);
    m_threadsTree->SetShowHeader(true);
    m_threadsTree->AddHeader(_("ID"));     // The thread/frame ID
    m_threadsTree->AddHeader(_("Name"));   // The frame source (function or file)
    m_threadsTree->AddHeader(_("Line"));   // The frame line
    m_threadsTree->AddHeader(_("Source")); // The thread/frame name
    m_threadsTree->AddRoot(_("Threads"));
    m_threadsTree->Bind(wxEVT_TREE_ITEM_EXPANDING, &DAPMainView::OnThreadItemExpanding, this);

    m_variablesTree->SetTreeStyle(m_variablesTree->GetTreeStyle() | wxTR_HIDE_ROOT);
    m_variablesTree->SetShowHeader(true);
    m_variablesTree->AddHeader(_("Name"));
    m_variablesTree->AddHeader(_("Value"));
    m_variablesTree->AddHeader(_("Type"));
    m_variablesTree->AddRoot(_("Scopes"));
    m_variablesTree->Bind(wxEVT_TREE_ITEM_EXPANDING, &DAPMainView::OnScopeItemExpanding, this);
}

DAPMainView::~DAPMainView() {}

void DAPMainView::UpdateThreads(int activeThreadId, dap::ThreadsResponse* response)
{
    // threads are the top level
    if(!response || !response->success) {
        return;
    }

    // build a map that matches thread-id -> wxTreeItemId
    std::unordered_map<int, wxTreeItemId> M;
    wxTreeItemId root = m_threadsTree->GetRootItem();
    wxTreeItemIdValue cookie;
    auto curitem = m_threadsTree->GetFirstChild(root, cookie);
    while(curitem.IsOk()) {
        M.insert({ ::wxStringToInt(m_threadsTree->GetItemText(curitem, 0), -1), curitem });
        curitem = m_threadsTree->GetNextChild(root, cookie);
    }

    m_threadsTree->Begin();
    for(const auto& thread : response->threads) {
        wxTreeItemId item;
        if(M.count(thread.id) == 0) {
            // new thread, add it
            item = m_threadsTree->AppendItem(root, wxString() << thread.id);
            // add a dummy child, so will get the ">" button
            m_threadsTree->AppendItem(item, "<dummy>");
        } else {
            item = M[thread.id];
            M.erase(thread.id); // remove it
        }
        if(thread.id == activeThreadId) {
            m_threadsTree->SetItemBold(item, true, 0);
            m_threadsTree->SetItemBold(item, true, 1);
        }
        m_threadsTree->SetItemText(item, thread.name, 1);
    }

    // remove any item left in the Map
    for(const auto& vt : M) {
        m_threadsTree->Delete(vt.second);
    }
    M.clear();
    m_threadsTree->Commit();
}

wxTreeItemId DAPMainView::FindThreadNode(int threadId)
{
    wxTreeItemId root = m_threadsTree->GetRootItem();
    wxTreeItemIdValue cookie;
    auto curitem = m_threadsTree->GetFirstChild(root, cookie);
    while(curitem.IsOk()) {
        int cur_thread_id = ::wxStringToInt(m_threadsTree->GetItemText(curitem, 0), -1);
        if(cur_thread_id == threadId) {
            return curitem;
        }
        curitem = m_threadsTree->GetNextChild(root, cookie);
    }
    return wxTreeItemId(nullptr);
}

void DAPMainView::UpdateScopes(int frameId, dap::ScopesResponse* response)
{
    LOG_DEBUG(LOG) << "Current frame id:" << m_scopesFrameId << endl;
    LOG_DEBUG(LOG) << "udpating scopes for frame:" << frameId << endl;
    bool changed_frame = m_scopesFrameId != frameId;
    m_scopesFrameId = frameId;

    m_variablesTree->Begin();
    if(changed_frame) {
        m_variablesTree->DeleteChildren(m_variablesTree->GetRootItem());
    }

    // go over the tree and collect the current names
    std::unordered_map<wxString, wxTreeItemId> current_scopes;
    wxTreeItemIdValue cookie;
    wxTreeItemId root = m_variablesTree->GetRootItem();
    auto curitem = m_variablesTree->GetFirstChild(root, cookie);
    while(curitem.IsOk()) {
        current_scopes.insert({ m_variablesTree->GetItemText(curitem), curitem });
        curitem = m_variablesTree->GetNextChild(root, cookie);
    }

    for(const auto& scope : response->scopes) {
        if(current_scopes.count(scope.name) == 0) {
            // new scope
            auto item = m_variablesTree->AppendItem(root, scope.name);
            m_variablesTree->SetItemData(item, new VariableClientData(scope.variablesReference));
            if(scope.variablesReference > 0) {
                // it has children
                m_variablesTree->AppendItem(item, "<dummy>");
            }
        } else {
            // remove it
            current_scopes.erase(scope.name);
        }
    }

    // remove all items that are still in the map
    for(const auto& vt : current_scopes) {
        m_variablesTree->Delete(vt.second);
    }
    m_variablesTree->Commit();
}

void DAPMainView::UpdateVariables(int parentRef, dap::VariablesResponse* response)
{
    // go over the tree and collect the current names
    wxTreeItemId parent_item = FindVariableNode(parentRef);
    CHECK_ITEM_RET(parent_item);

    m_variablesTree->Begin();
    m_variablesTree->DeleteChildren(parent_item);

    for(const auto& variable : response->variables) {
        auto child = m_variablesTree->AppendItem(parent_item, variable.name);
        m_variablesTree->SetItemText(child, variable.value, 1);
        m_variablesTree->SetItemText(child, variable.type, 2);
        m_variablesTree->SetItemData(child, new VariableClientData(variable.variablesReference));
        if(variable.variablesReference > 0) {
            // has children
            m_variablesTree->AppendItem(child, "<dummy>");
        }
    }
    m_variablesTree->Commit();
}

void DAPMainView::UpdateFrames(int threadId, dap::StackTraceResponse* response)
{
    // locate the row
    m_threadsTree->Begin();
    wxTreeItemId parent = FindThreadNode(threadId);
    if(!parent.IsOk()) {
        return;
    }

    if(m_threadsTree->ItemHasChildren(parent)) {
        m_threadsTree->DeleteChildren(parent);
    }

    // append the stack frame
    for(const auto& frame : response->stackFrames) {
        wxTreeItemId frame_item = m_threadsTree->AppendItem(parent, wxString() << frame.id);

        wxString source;
        if(!frame.source.path.empty()) {
            source = frame.source.path;
        } else {
            source = frame.source.name;
        }

        m_threadsTree->SetItemText(frame_item, source, 1);
        m_threadsTree->SetItemText(frame_item, wxString() << frame.line, 2);
        m_threadsTree->SetItemText(frame_item, frame.name, 3);
    }
    m_threadsTree->Commit();
    m_threadsTree->Expand(parent);
}

void DAPMainView::OnScopeItemExpanding(wxTreeEvent& event)
{
    event.Skip();
    wxTreeItemId item = event.GetItem();
    CHECK_ITEM_RET(item);

    wxTreeItemIdValue cookie;
    m_variablesTree->Begin();
    if(m_variablesTree->ItemHasChildren(item)) {
        m_variablesTree->DeleteChildren(item);
        m_variablesTree->AppendItem(item, _("Loading..."));
    }
    m_variablesTree->Commit();

    // delete the children and request for backtrace
    m_client->GetChildrenVariables(GetVariableId(item));
}

void DAPMainView::OnThreadItemExpanding(wxTreeEvent& event)
{
    event.Skip();
    wxTreeItemId item = event.GetItem();
    CHECK_ITEM_RET(item);

    wxTreeItemIdValue cookie;
    if(m_threadsTree->ItemHasChildren(item)) {
        m_threadsTree->DeleteChildren(item);
        m_threadsTree->AppendItem(item, _("Loading..."));
    }

    // delete the children and request for backtrace
    m_client->GetFrames(GetThreadId(item));
}

int DAPMainView::GetVariableId(const wxTreeItemId& item)
{
    if(!item.IsOk()) {
        return wxNOT_FOUND;
    }
    VariableClientData* cd = dynamic_cast<VariableClientData*>(m_variablesTree->GetItemData(item));
    if(cd) {
        return cd->reference;
    }
    return wxNOT_FOUND;
}

int DAPMainView::GetThreadId(const wxTreeItemId& item)
{
    if(!item.IsOk()) {
        return wxNOT_FOUND;
    }
    return wxStringToInt(m_threadsTree->GetItemText(item), wxNOT_FOUND);
}

std::unordered_set<int> DAPMainView::GetExpandedThreads()
{
    std::unordered_set<int> result = { m_client->GetActiveThreadId() };
    wxTreeItemIdValue cookie;
    wxTreeItemId root = m_threadsTree->GetRootItem();
    auto curitem = m_threadsTree->GetFirstChild(root, cookie);
    while(curitem.IsOk()) {
        if(m_threadsTree->IsExpanded(curitem)) {
            int cur_thread_id = ::wxStringToInt(m_threadsTree->GetItemText(curitem, 0), -1);
            result.insert(cur_thread_id);
        }
        curitem = m_threadsTree->GetNextChild(root, cookie);
    }
    return result;
}

// O(n)
wxTreeItemId DAPMainView::FindVariableNode(int refId)
{
    wxTreeItemId parent = m_variablesTree->GetRootItem();
    std::vector<wxTreeItemId> Q;
    // scan in dfs manner
    Q.push_back(parent);
    while(!Q.empty()) {
        wxTreeItemId item = Q.back();
        Q.pop_back();

        int curid = GetVariableId(item);
        if(curid == refId) {
            return item;
        }

        if(m_variablesTree->HasChildren(item)) {
            // direct access to this item children goes through clRowEntry (the internal impl)
            clRowEntry* as_entry = reinterpret_cast<clRowEntry*>(item.GetID());
            const auto& children = as_entry->GetChildren();
            Q.reserve(Q.size() + children.size());
            for(auto child : children) {
                Q.emplace_back(wxTreeItemId(child));
            }
        }
    }
    return wxTreeItemId(nullptr);
}

void DAPMainView::Clear()
{
    m_variablesTree->DeleteAllItems();
    m_threadsTree->DeleteAllItems();
}
