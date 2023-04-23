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

/*static*/const std::string CtStorageMultiFile::SUBNODES_XML{"subnodes.xml"};
/*static*/const std::string CtStorageMultiFile::NODE_XML{"node.xml"};

bool CtStorageMultiFile::populate_treestore(const fs::path& file_path, Glib::ustring& error)
{
    //TODO
    return false;
}

bool CtStorageMultiFile::save_treestore(const fs::path& dir_path,
                                        const CtStorageSyncPending& syncPending,
                                        Glib::ustring& error,
                                        const CtExporting exporting/*= CtExporting::NONE*/,
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

            xmlpp::Document xml_doc_subnodes;
            xml_doc_subnodes.create_root_node(CtConst::APP_NAME);

            if ( CtExporting::NONE == exporting or
                 CtExporting::ALL_TREE == exporting )
            {
                // save bookmarks
                xmlpp::Element* p_bookmarks_node = xml_doc_subnodes.get_root_node()->add_child("bookmarks");
                p_bookmarks_node->set_attribute("list", str::join_numbers(_pCtMainWin->get_tree_store().bookmarks_get(), ","));
            }

            CtStorageCache storage_cache;
            storage_cache.generate_cache(_pCtMainWin, nullptr/*all nodes*/, false/*for_xml*/);

            std::list<gint64> subnodes;

            // save nodes
            if ( CtExporting::NONE == exporting or
                 CtExporting::ALL_TREE == exporting ) {
                auto ct_tree_iter = _pCtMainWin->get_tree_store().get_ct_iter_first();
                while (ct_tree_iter) {
                    subnodes.push_back(ct_tree_iter.get_node_id());
                    if (not _nodes_to_multifile(&ct_tree_iter, dir_path, error, &storage_cache, exporting, start_offset, end_offset)) {
                        return false;
                    }
                    ct_tree_iter++;
                }
            }
            else {
                CtTreeIter ct_tree_iter = _pCtMainWin->curr_tree_iter();
                subnodes.push_back(ct_tree_iter.get_node_id());
                if (not _nodes_to_multifile(&ct_tree_iter, dir_path, error, &storage_cache, exporting, start_offset, end_offset)) {
                    return false;
                }
            }

            // save subnodes
            xmlpp::Element* p_bookmarks_node = xml_doc_subnodes.get_root_node()->add_child("subnodes");
            p_bookmarks_node->set_attribute("list", str::join_numbers(subnodes, ","));

            // write file
            xml_doc_subnodes.write_to_file_formatted(Glib::build_filename(dir_path.string(), SUBNODES_XML));
        }
        else {
            // or need just update some info
            //TODO
        }
        return true;
    }
    catch (std::exception& e) {
        error = e.what();
        return false;
    }
}

bool CtStorageMultiFile::_nodes_to_multifile(CtTreeIter* ct_tree_iter,
                                             const fs::path& parent_dir_path,
                                             Glib::ustring& error,
                                             CtStorageCache* storage_cache,
                                             const CtExporting exporting/*= CtExporting::NONE*/,
                                             const int start_offset/*= 0*/,
                                             const int end_offset/*=-1*/)
{
    const fs::path dir_path = parent_dir_path / std::to_string(ct_tree_iter->get_node_id());
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
    if ( CtExporting::CURRENT_NODE != exporting and
         CtExporting::SELECTED_TEXT != exporting )
    {
        CtTreeIter ct_tree_iter_child = ct_tree_iter->first_child();
        if (ct_tree_iter_child) {

            xmlpp::Document xml_doc_subnodes;
            xml_doc_subnodes.create_root_node(CtConst::APP_NAME);
            std::list<gint64> subnodes;

            while (true) {
                subnodes.push_back(ct_tree_iter_child.get_node_id());
                if (not _nodes_to_multifile(&ct_tree_iter_child, dir_path, error, storage_cache, exporting, start_offset, end_offset)) {
                    return false;
                }
                ct_tree_iter_child++;
                if (not ct_tree_iter_child) {
                    break;
                }
            }

            // save subnodes
            xmlpp::Element* p_bookmarks_node = xml_doc_subnodes.get_root_node()->add_child("subnodes");
            p_bookmarks_node->set_attribute("list", str::join_numbers(subnodes, ","));

            // write file
            xml_doc_subnodes.write_to_file_formatted(Glib::build_filename(dir_path.string(), SUBNODES_XML));
        }
    }
    return true;
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
