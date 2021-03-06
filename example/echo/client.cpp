// Copyright (c) 2014 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A client sending requests to server every 1 second.

#include <gflags/gflags.h>
#include "butil/logging.h"
#include "butil/time.h"
#include "brpc/channel.h"
#include "brpc/cmd_flags.h"
#include "brpc/naming_service_filter.h"
#include "echo/echo.pb.h"


DEFINE_string(ns_url, "etcdns://sogou.nlu.rpc.example.EchoService", "IP Address of server");
DEFINE_int32(interval_ms, 1000, "Milliseconds between consecutive requests");


int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
    
    // Filter
    brpc::DefaultBasicServiceFilter stageFilter("stage","beta");
    brpc::DefaultBasicServiceFilter versionFilter("version","1.0");
    brpc::DefaultNamingServiceFilter namingServiceFilter;
    namingServiceFilter.RegisterBasicFilter(&stageFilter);
    namingServiceFilter.RegisterBasicFilter(&versionFilter);

    // A Channel represents a communication line to a Server. Notice that 
    // Channel is thread-safe and can be shared by all threads in your program.
    brpc::Channel channel;

    // Initialize the channel, NULL means using default options.
    brpc::ChannelOptions options;
    options.protocol = FLAGS_protocol;
    options.connection_type = FLAGS_connection_type;
    options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
    options.max_retry = FLAGS_max_retry;
    options.ns_filter = &namingServiceFilter;
    if (channel.Init(FLAGS_ns_url.c_str(), FLAGS_load_balancer.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    // Normally, you should not call a Channel directly, but instead construct
    // a stub Service wrapping it. stub can be shared by all threads as well.
    sogou::nlu::rpc::example::EchoService_Stub stub(&channel);

    // Send a request and wait for the response every 1 second.
    int log_id = 0;
    while (!brpc::IsAskedToQuit()) {
        // We will receive response synchronously, safe to put variables
        // on stack.
        sogou::nlu::rpc::example::EchoRequest request;
        sogou::nlu::rpc::example::EchoResponse response;
        brpc::Controller cntl;

        request.set_message("hello world");

        cntl.set_log_id(log_id ++);  // set by user
        if (FLAGS_protocol != "http" && FLAGS_protocol != "h2c")  {
            // Set attachment which is wired to network directly instead of 
            // being serialized into protobuf messages.
            //cntl.request_attachment().append(FLAGS_attachment);
        } else {
            cntl.http_request().set_content_type(FLAGS_http_content_type);
        }

        // Because `done'(last parameter) is NULL, this function waits until
        // the response comes back or error occurs(including timedout).
        stub.echo(&cntl, &request, &response, NULL);
        if (!cntl.Failed()) {
            if (cntl.response_attachment().empty()) {
                LOG(INFO) << "Received response from " << cntl.remote_side()
                          << ": " << response.message()
                          << " latency=" << cntl.latency_us() << "us";
            } else {
                LOG(INFO) << "Received response from " << cntl.remote_side()
                          << " to " << cntl.local_side()
                          << ": " << response.message() << " (attached="
                          << cntl.response_attachment() << ")"
                          << " latency=" << cntl.latency_us() << "us";
            }
        } else {
            LOG(WARNING) << cntl.ErrorText();
        }
        usleep(FLAGS_interval_ms * 1000L);
    }

    LOG(INFO) << "EchoClient is going to quit";
    return 0;
}
