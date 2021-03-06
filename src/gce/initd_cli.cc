// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "gflags/gflags.h"
#include "proto/agent.pb.h"
#include "proto/initd.pb.h"
#include "rpc/rpc_client.h"
#include "tprinter.h"

bool TerminateContact(int fdm) {
    if (fdm < 0) {
        return false; 
    }
    struct termios temp_termios;
    struct termios orig_termios;

    ::tcgetattr(0, &orig_termios);
    temp_termios = orig_termios;
    // 去掉输入同步的输出, 以及不等待换行符
    temp_termios.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    temp_termios.c_cc[VTIME] = 1;   // 终端等待延迟时间（十分之一秒为单位）
    temp_termios.c_cc[VMIN] = 1;    // 终端接收字符数
    ::tcsetattr(0, TCSANOW, &temp_termios);

    const int INPUT_BUFFER_LEN = 1024 * 10;
    char input[INPUT_BUFFER_LEN];
    fd_set fd_in; 
    int ret = 0;
    while (1) {
        FD_ZERO(&fd_in); 
        FD_SET(0, &fd_in);
        FD_SET(fdm, &fd_in);
        ret = ::select(fdm + 1, &fd_in, NULL, NULL, NULL);
        switch (ret) {
            case -1 :  
                fprintf(stderr, "select err[%d: %s]\n", 
                        errno, strerror(errno));
                break;
            default : {
                if (FD_ISSET(0, &fd_in))  {
                    ret = ::read(0, input, sizeof(input)); 
                    if (ret > 0) {
                        ::write(fdm, input, ret); 
                    } else {
                        if (ret < 0) {
                            fprintf(stderr, "read err[%d: %s]\n",
                                    errno,
                                    strerror(errno)); 
                            break;
                        }
                    }
                }         
                if (FD_ISSET(fdm, &fd_in)) {
                    ret = ::read(fdm, input, sizeof(input)); 
                    if (ret > 0) {
                        ::write(1, input, ret);
                    } else {
                        if (ret < 0) {
                            fprintf(stderr, "read err[%d: %s]\n",
                                    errno,
                                    strerror(errno)); 
                            break;
                        } 
                    }
                }
            }
        }
        if (ret < 0) {
            break; 
        }
    }

    ::tcsetattr(0, TCSANOW, &orig_termios);
    if (ret < 0) {
        return false; 
    }
    return true;
} 

bool PreparePty(int* fdm, std::string* pty_file) {
    if (pty_file == NULL || fdm == NULL) {
        return false; 
    }
    *fdm = -1;
    *fdm = ::posix_openpt(O_RDWR);
    if (*fdm < 0) {
        fprintf(stderr, "posix_openpt err[%d: %s]",
                errno, strerror(errno)); 
        return false;
    }

    int ret = ::grantpt(*fdm);
    if (ret != 0) {
        fprintf(stderr, "grantpt err[%d: %s]", 
                errno, strerror(errno)); 
        ::close(*fdm);
        return false;
    }

    ret = ::unlockpt(*fdm);
    if (ret != 0) {
        fprintf(stderr, "unlockpt err[%d: %s]", 
                errno, strerror(errno));
        ::close(*fdm);
        return false;
    }
    pty_file->clear();
    pty_file->append(::ptsname(*fdm));
    return true;
}

DEFINE_string(initd_endpoint, "", "initd endpoint");
DEFINE_string(user, "", "use user");
DEFINE_string(chroot, "", "chroot path");
DEFINE_string(LINES, "39", "env values");
DEFINE_string(COLUMNS, "139", "env values");

int main(int argc, char* argv[]) {
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    baidu::galaxy::Initd_Stub* initd;
    baidu::galaxy::RpcClient* rpc_client = 
        new baidu::galaxy::RpcClient();
    std::string endpoint(FLAGS_initd_endpoint);
    rpc_client->GetStub(endpoint, &initd);

    std::string pty_file;
    int pty_fdm = -1;
    if (!PreparePty(&pty_fdm, &pty_file)) {
        fprintf(stderr, "prepare pty failed\n"); 
        return -1;
    }

    baidu::galaxy::ExecuteRequest exec_request;
    // TODO unqic key 
    exec_request.set_key("client");
    exec_request.set_commands("/bin/bash");
    exec_request.set_path(".");
    exec_request.set_pty_file(pty_file);
    if (FLAGS_user != "") {
        exec_request.set_user(FLAGS_user);
    }
    if (FLAGS_chroot != "") {
        exec_request.set_chroot_path(FLAGS_chroot); 
    }
    std::string* lines_env = exec_request.add_envs();
    lines_env->append("LINES=");
    lines_env->append(FLAGS_LINES);
    std::string* columns_env = exec_request.add_envs();
    columns_env->append("COLUMNS=");
    columns_env->append(FLAGS_COLUMNS);
    baidu::galaxy::ExecuteResponse exec_response;
    bool ret = rpc_client->SendRequest(initd,
                            &baidu::galaxy::Initd_Stub::Execute,
                            &exec_request,
                            &exec_response, 5, 1);
    if (ret && exec_response.status() == baidu::galaxy::kOk) {
        fprintf(stdout, "terminate starting...\n");
        ret = TerminateContact(pty_fdm); 
        if (ret) {
            fprintf(stdout, "terminate contact over\n"); 
        } else {
            fprintf(stderr, "terminate contact interrupt\n"); 
        }
        return 0;
    } 
    fprintf(stderr, "exec in initd failed %s\n", 
            baidu::galaxy::Status_Name(exec_response.status()).c_str());
    return -1;
}

/* vim: set ts=4 sw=4 sts=4 tw=100 */
