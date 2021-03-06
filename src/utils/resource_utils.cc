// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils/resource_utils.h"

#include <set>
#include "logging.h"

namespace baidu {
namespace galaxy {

static bool CompareVolumeDesc(const Volume& l, const Volume& r) {
    return l.quota() > r.quota();
}

static bool CompareVolumeAsc(const Volume& l, const Volume& r) {
    return l.quota() < r.quota();
}

bool ResourceUtils::GtOrEq(const Resource& left,
                           const Resource& right) {
    return Compare(left, right) == -1 ? false : true;
}


bool ResourceUtils::Alloc(const Resource& require, 
                          Resource& target) {
    // target should be >= target
    if (!GtOrEq(target, require)) {
        return false;
    }

    target.set_millicores(target.millicores() - require.millicores());
    target.set_memory(target.memory() - require.memory());
    VolumeAlloc(target.disks(), require.disks(), 
                target.mutable_disks());
    VolumeAlloc(target.ssds(), require.ssds(),
                target.mutable_ssds());
    return true;
}

void ResourceUtils::VolumeAlloc(const VolumeList& total,
                                const VolumeList& require,
                                VolumeList* left) {

    std::vector<Volume> v_total;
    std::vector<Volume> v_require;
    for (int i = 0; i < total.size(); i++) {
        v_total.push_back(total.Get(i));
    }

    for (int i = 0; i < require.size(); i++) {
        v_require.push_back(require.Get(i));
    }
    std::sort(v_total.begin(), v_total.end(), CompareVolumeAsc);
    std::sort(v_require.begin(), v_require.end(), CompareVolumeAsc);
    left->Clear();
    size_t require_index = 0;
    for (size_t i = 0; i < v_total.size(); i++) {
        if (i <= require_index) {
            if (v_require[require_index].quota() > v_total[i].quota()) {
                left->Add()->CopyFrom(v_total[i]);
            }
        } else{
            left->Add()->CopyFrom(v_total[i]);
        } 
    }
}



int32_t ResourceUtils::Compare(const Resource& left,
                               const Resource& right) {
    int32_t cpu_check = 0;
    if (left.millicores() > right.millicores()) {
        cpu_check = 1;
    }else if (left.millicores() < right.millicores()) {
        return -1;
    }

    int32_t mem_check = 0;
    if (left.memory() > right.memory()) {
        mem_check = 1;
    }else if (left.memory() < right.memory()) {
        return -1;
    }

    std::vector<Volume> left_disks;
    for (int i = 0; i < left.disks_size(); i++) {
        left_disks.push_back(left.disks(i));
    }
    std::vector<Volume> right_disks;
    for (int i = 0; i < right.disks_size(); i++) {
        right_disks.push_back(right.disks(i));
    }

    int32_t disk_check = CompareVector(left_disks, 
                                       right_disks,
                                       CompareVolumeDesc);
    if (disk_check == -1) {
        return -1;
    }

    std::vector<Volume> left_ssds;
    for (int i = 0; i < left.ssds_size(); i++) {
        left_ssds.push_back(left.ssds(i));
    }
    std::vector<Volume> right_ssds;
    for (int i = 0; i < right.ssds_size(); i++) {
        right_ssds.push_back(right.ssds(i));
    }

    int32_t ssd_check = CompareVector(left_ssds, 
                                      right_ssds,
                                      CompareVolumeDesc);
    if (ssd_check == -1) {
        return -1;
    }
    return cpu_check & mem_check & disk_check & ssd_check;
}


} // galaxy
} // baidu
