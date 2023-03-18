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

bool CtStorageMultiFile::populate_treestore(const fs::path& file_path, Glib::ustring& error)
{
    //TODO
    return false;
}

bool CtStorageMultiFile::save_treestore(const fs::path& file_path,
                                        const CtStorageSyncPending& syncPending,
                                        Glib::ustring& error,
                                        const CtExporting exporting/*= CtExporting::NONE*/,
                                        const int start_offset/*= 0*/,
                                        const int end_offset/*= -1*/)
{
    //TODO
    return false;
}

void CtStorageMultiFile::import_nodes(const fs::path& path, const Gtk::TreeIter& parent_iter)
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
