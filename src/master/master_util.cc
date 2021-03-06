// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "master_util.h"

#include <sstream>
#include <sys/utsname.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <gflags/gflags.h>
#include <logging.h>

#include "proto/galaxy.pb.h"
#include "proto/master.pb.h"

DECLARE_string(master_port);

namespace baidu {
namespace galaxy {

std::string MasterUtil::UUID() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::lexical_cast<std::string>(uuid); 
}


void MasterUtil::AddResource(const Resource& from, Resource* to) {
    to->set_millicores(to->millicores() + from.millicores());
    to->set_memory(to->memory() + from.memory());
}

void MasterUtil::SubstractResource(const Resource& from, Resource* to) {
    assert(FitResource(from, *to));
    to->set_millicores(to->millicores() - from.millicores());
    to->set_memory(to->memory() - from.memory());
}

bool MasterUtil::FitResource(const Resource& from, const Resource& to) {
    if (to.millicores() < from.millicores()) {
        return false;
    }
    if (to.memory() < from.memory()) {
        return false;
    }
    // TODO: check port & disk & ssd
    return true;
}

std::string MasterUtil::SelfEndpoint() {
    std::string hostname = "";
    struct utsname buf;
    if (0 != uname(&buf)) {
        *buf.nodename = '\0';
    }
    hostname = buf.nodename;
    return hostname + ":" + FLAGS_master_port;
}

void MasterUtil::TraceJobDesc(const JobDescriptor& job_desc) {
    LOG(INFO, "job descriptor: \"%s\", "
              "replica:%d, "
              "deploy_step:%d",
              job_desc.name().c_str(),
              job_desc.replica(),
              job_desc.deploy_step()
    );
    LOG(INFO, "pod descriptor mem:%ld, cpu:%d, port_size:%d",
       job_desc.pod().requirement().memory(),
       job_desc.pod().requirement().millicores(),
       job_desc.pod().requirement().ports_size());
    for (int i = 0; i < job_desc.pod().tasks_size(); i++) {
        const TaskDescriptor& task_desc = job_desc.pod().tasks(i);
        LOG(INFO, "job[%s]:task[%d] descriptor: "
            "start_cmd: \"%s\", stop_cmd: \"%s\", binary_size:%d, mem:%ld, cpu:%d ",
            job_desc.name().c_str(),
            i, 
            task_desc.start_command().c_str(),
            task_desc.stop_command().c_str(),
            task_desc.binary().size(),
            task_desc.requirement().memory(),
            task_desc.requirement().millicores()
        );
    }
}

void MasterUtil::SetDiff(const std::set<std::string>& left_set,
                         const std::set<std::string>& right_set,
                         std::set<std::string>* left_diff,
                         std::set<std::string>* right_diff) {
    if (left_diff == NULL || right_diff == NULL) {
        LOG(WARNING, "set diff error for input NULL");
        return; 
    }

    if (left_set.size() == 0) {
        *right_diff = right_set;
        return;
    }

    if (right_set.size() == 0) {
        *left_diff = left_set; 
        return;
    }
    std::set<std::string>::iterator it_left = left_set.begin();    
    std::set<std::string>::iterator it_right = right_set.begin();
    while (it_left != left_set.end() 
            && it_right != right_set.end()) {
        if (*it_left == *it_right) {
            ++it_left;
            ++it_right;
        } else if (*it_left > *it_right) {
            right_diff->insert(*it_right);
            ++it_right;
        } else {
            left_diff->insert(*it_left);
            ++it_left;
        }
    }

    for (; it_left != left_set.end(); ++it_left) {
        left_diff->insert(*it_left); 
    }
    for (; it_right != right_set.end(); ++it_right) {
        right_diff->insert(*it_right); 
    }
    return;
}

void MasterUtil::ResetLabels(AgentInfo* agent, 
                             const std::set<std::string>& labels) {
    if (agent == NULL) {
        return; 
    }

    agent->clear_tags();
    std::set<std::string>::iterator it = labels.begin();
    for (; it != labels.end(); ++it) {
        agent->add_tags(*it); 
    }
    return;
}

}
}
