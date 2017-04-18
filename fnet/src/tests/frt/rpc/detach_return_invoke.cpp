// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include <vespa/vespalib/testkit/test_kit.h>
#include <vespa/fnet/frt/frt.h>
#include <vespa/vespalib/util/stringfmt.h>

struct Receptor : public FRT_IRequestWait
{
    FRT_RPCRequest *req;

    Receptor() : req(0) {}
    void RequestDone(FRT_RPCRequest *r) override {
        req = r;
    }
};

struct Server : public FRT_Invokable
{
    FRT_Supervisor &orb;
    Receptor       &receptor;

    Server(FRT_Supervisor &s, Receptor &r) : orb(s), receptor(r) {
        FRT_ReflectionBuilder rb(&s);
        rb.DefineMethod("hook", "", "", true,
                        FRT_METHOD(Server::rpc_hook), this);
    }

    void rpc_hook(FRT_RPCRequest *req) {
        FNET_Connection *conn = req->GetConnection();
        conn->AddRef(); // need to keep it alive
        req->Detach();
        req->Return();  // will free request channel
        FRT_RPCRequest *r = orb.AllocRPCRequest();
        r->SetMethodName("frt.rpc.ping");
        // might re-use request channel before it is unlinked from hashmap
        orb.InvokeAsync(orb.GetTransport(), conn, r, 5.0, &receptor);
        conn->SubRef(); // invocation will now keep the connection alive as needed
    }
};

TEST("detach return invoke") {
    Receptor receptor;
    FRT_Supervisor orb;
    Server server(orb, receptor);
    ASSERT_TRUE(orb.Listen(0));
    ASSERT_TRUE(orb.Start());
    std::string spec =  vespalib::make_string("tcp/localhost:%d", orb.GetListenPort());
    FRT_Target *target = orb.Get2WayTarget(spec.c_str());
    FRT_RPCRequest *req = orb.AllocRPCRequest();

    req->SetMethodName("hook");
    target->InvokeSync(req, 5.0);
    EXPECT_TRUE(!req->IsError());
    for (uint32_t i = 0; i < 1000; ++i) {
        if (receptor.req != 0) {
            break;
        }
        FastOS_Thread::Sleep(10);
    }
    req->SubRef();
    target->SubRef();
    orb.ShutDown(true);
    if (receptor.req != 0) {
        EXPECT_TRUE(!receptor.req->IsError());
        receptor.req->SubRef();
    }
    EXPECT_TRUE(receptor.req != 0);
};

TEST_MAIN() { TEST_RUN_ALL(); }
