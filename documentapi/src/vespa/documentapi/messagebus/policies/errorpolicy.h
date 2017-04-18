// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include <vespa/messagebus/routing/iroutingpolicy.h>

namespace documentapi {

/**
 * This policy assigns an error supplied at constructor time to the routing context when {@link
 * #select(RoutingContext)} is invoked. This is useful for returning error states to the client instead of
 * those auto-generated by mbus when a routing policy can not be created.
 */
class ErrorPolicy : public mbus::IRoutingPolicy {
private:
    string _msg;

public:
    /**
     * Creates a new policy that will assign an {@link EmptyReply} with the given error to all routing
     * contexts that invoke {@link #select(RoutingContext)}.
     *
     * @param msg The message of the error to assign.
     */
    ErrorPolicy(const string &msg);

    // Implements IRoutingPolicy.
    void select(mbus::RoutingContext &context) override;

    // Implements IRoutingPolicy.
    void merge(mbus::RoutingContext &context) override;
};

}

