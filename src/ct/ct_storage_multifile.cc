/*
 * ct_storage_multifile.cc
 *
 * Copyright 2009-2023
 * Giuseppe Penone <giuspen@gmail.com>
 * Evgenii Gurianov <https://github.com/txe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "ct_storage_multifile.h"
#include "ct_storage_xml.h"
#include "ct_storage_control.h"
#include "ct_main_win.h"
#include "ct_logging.h"
#include <glib/gstdio.h>

/*static*/const std::string CtStorageMultiFile::SUBNODES_LST{"subnodes.lst"};
/*static*/const std::string CtStorageMultiFile::BOOKMARKS_LST{"bookmarks.lst"};
/*static*/const std::string CtStorageMultiFile::NODE_XML{"node.xml"};

bool CtStorageMultiFile::save_treestore(const fs::path& dir_path,
                                        const CtStorageSyncPending& syncPending,
                                        Glib::ustring& error,
                                        const CtExporting exporting,
                                        const int start_offset/*= 0*/,
                                        const int end_offset/*= -1*/)
{
    try {
        if (_dir_path.empty()) {
            // it's the first time (or an export), a new folder will be created
            if (fs::is_directory(dir_path)) {
                auto message = str::format(_("A folder with name '%s' already exists in '%s'.\n<b>Do you want to remove it?</b>"),
                    str::xml_escape(dir_path.filename().string()), str::xml_escape(dir_path.parent_path().string()));
                if (not CtDialogs::question_dialog(message, *_pCtMainWin)) {
                    return false;
                }
                (void)fs::remove_all(dir_path);
                if (fs::is_directory(dir_path)) {
                    error = Glib::ustring{"failed to remove "} + dir_path.string();
                    return false;
                }
            }
            if (g_mkdir(dir_path.c_str(), 0755) < 0) {
                error = Glib::ustring{"failed to create "} + dir_path.string();
                return false;
            }
            _dir_path = dir_path;

            if ( CtExporting::NONESAVEAS == exporting or
                 CtExporting::ALL_TREE == exporting )
            {
                // save bookmarks
                _write_bookmarks_to_disk(_pCtMainWin->get_tree_store().bookmarks_get());
            }
            CtStorageNodeState node_state;
            node_state.is_update_of_existing = false; // no need to delete the prev data
            node_state.prop = true;
            node_state.buff = true;
            node_state.hier = true;

            CtStorageCache storage_cache;
            storage_cache.generate_cache(_pCtMainWin, nullptr/*all nodes*/, false/*for_xml*/);

            std::list<gint64> subnodes_list;

            // save nodes
            if ( CtExporting::NONESAVEAS == exporting or
                 CtExporting::ALL_TREE == exporting )
            {
                auto ct_tree_iter = _pCtMainWin->get_tree_store().get_ct_iter_first();
                while (ct_tree_iter) {
                    const auto node_id = ct_tree_iter.get_node_id();
                    subnodes_list.push_back(node_id);
                    if (not _nodes_to_multifile(&ct_tree_iter,
                                                dir_path / std::to_string(node_id),
                                                error,
                                                &storage_cache,
                                                node_state,
                                                exporting,
                                                start_offset,
                                                end_offset))
                    {
                        return false;
                    }
                    ct_tree_iter++;
                }
            }
            else {
                CtTreeIter ct_tree_iter = _pCtMainWin->curr_tree_iter();
                const auto node_id = ct_tree_iter.get_node_id();
                subnodes_list.push_back(node_id);
                if (not _nodes_to_multifile(&ct_tree_iter,
                                            dir_path / std::to_string(node_id),
                                            error,
                                            &storage_cache,
                                            node_state,
                                            exporting,
                                            start_offset,
                                            end_offset))
                {
                    return false;
                }
            }

            // save subnodes
            Glib::file_set_contents(Glib::build_filename(dir_path.string(), SUBNODES_LST),
                                    str::join_numbers(subnodes_list, ","));
        }
        else {
            // or need just update some info
            CtStorageCache storage_cache;
            storage_cache.generate_cache(_pCtMainWin, &syncPending, false/*for_xml*/);

            // update bookmarks
            if (syncPending.bookmarks_to_write) {
                _write_bookmarks_to_disk(_pCtMainWin->get_tree_store().bookmarks_get());
            }
            // update changed nodes
            for (const auto& node_pair : syncPending.nodes_to_write_dict) {
                CtTreeIter ct_tree_iter = _pCtMainWin->get_tree_store().get_node_from_node_id(node_pair.first);
                CtTreeIter ct_tree_iter_parent = ct_tree_iter.parent();
                _nodes_to_multifile(&ct_tree_iter,
                                    _get_node_dirpath(node_pair.first),
                                    error,
                                    &storage_cache,
                                    node_pair.second,
                                    exporting,
                                    0,
                                    -1);
            }
            // remove nodes and their sub nodes
            for (const auto node_id : syncPending.nodes_to_rm_set) {
                _remove_disk_node_with_children(node_id);
            }
        }
        return true;
    }
    catch (std::exception& e) {
        error = e.what();
        return false;
    }
}

fs::path CtStorageMultiFile::_get_node_dirpath(const gint64 node_id)
{
    CtTreeIter tree_iter = _pCtMainWin->get_tree_store().get_node_from_node_id(node_id);
    if (not tree_iter) {
        return fs::path{};
    }
    fs::path hierarchical_path{std::to_string(node_id)};
    CtTreeIter father_iter = tree_iter.parent();
    while (father_iter) {
        hierarchical_path = fs::path{std::to_string(father_iter.get_node_id())} / hierarchical_path;
        father_iter = father_iter.parent();
    }
    return _dir_path / hierarchical_path;
}

void CtStorageMultiFile::_remove_disk_node_with_children(const gint64 node_id)
{
    const fs::path node_dirpath = _get_node_dirpath(node_id);
    (void)fs::remove_all(node_dirpath);
}

void CtStorageMultiFile::_write_bookmarks_to_disk(const std::list<gint64>& bookmarks_list)
{
    const fs::path bookmarks_filepath = _dir_path / BOOKMARKS_LST;
    (void)fs::remove(bookmarks_filepath);
    if (not bookmarks_list.empty()) {
        Glib::file_set_contents(bookmarks_filepath.string(),
                                str::join_numbers(bookmarks_list, ","));
    }
}

bool CtStorageMultiFile::_nodes_to_multifile(CtTreeIter* ct_tree_iter,
                                             const fs::path& dir_path,
                                             Glib::ustring& error,
                                             CtStorageCache* storage_cache,
                                             const CtStorageNodeState& node_state,
                                             const CtExporting exporting,
                                             const int start_offset/*= 0*/,
                                             const int end_offset/*=-1*/)
{
    // TODO support CtExporting::NONESAVE and node_state
    if (g_mkdir(dir_path.c_str(), 0755) < 0) {
        error = Glib::ustring{"failed to create "} + dir_path.string();
        return false;
    }
    {
        xmlpp::Document xml_doc_node;
        xml_doc_node.create_root_node(CtConst::APP_NAME);

        (void)CtStorageXmlHelper{_pCtMainWin}.node_to_xml(
            ct_tree_iter,
            xml_doc_node.get_root_node(),
            dir_path.string()/*multifile_dir*/,
            storage_cache,
            start_offset,
            end_offset
        );

        // write file
        xml_doc_node.write_to_file_formatted(Glib::build_filename(dir_path.string(), NODE_XML));
    }
    if ( CtExporting::NONESAVE != exporting and
         CtExporting::CURRENT_NODE != exporting and
         CtExporting::SELECTED_TEXT != exporting )
    {
        CtTreeIter ct_tree_iter_child = ct_tree_iter->first_child();
        if (ct_tree_iter_child) {

            std::list<gint64> subnodes_list;

            while (true) {
                subnodes_list.push_back(ct_tree_iter_child.get_node_id());
                if (not _nodes_to_multifile(&ct_tree_iter_child,
                                            dir_path,
                                            error,
                                            storage_cache,
                                            node_state,
                                            exporting,
                                            start_offset,
                                            end_offset))
                {
                    return false;
                }
                ct_tree_iter_child++;
                if (not ct_tree_iter_child) {
                    break;
                }
            }

            // save subnodes
            Glib::file_set_contents(Glib::build_filename(dir_path.string(), SUBNODES_LST),
                                    str::join_numbers(subnodes_list, ","));
        }
    }
    return true;
}

bool CtStorageMultiFile::populate_treestore(const fs::path& file_path, Glib::ustring& error)
{
    //TODO
    return false;
}

void CtStorageMultiFile::import_nodes(const fs::path& file_path, const Gtk::TreeIter& parent_iter)
{
    //TODO
}

Glib::RefPtr<Gsv::Buffer> CtStorageMultiFile::get_delayed_text_buffer(const gint64& node_id,
                                                                      const std::string& syntax,
                                                                      std::list<CtAnchoredWidget*>& widgets) const
{
    //TODO
    return Glib::RefPtr<Gsv::Buffer>{};
}
