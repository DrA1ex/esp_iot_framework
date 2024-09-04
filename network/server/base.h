#pragma once

#include "../../base/application.h"
#include "../protocol/binary.h"
#include "../protocol/type.h"

template<typename ApplicationT, typename = std::enable_if_t<std::is_base_of_v<
        AbstractApplication<typename ApplicationT::ConfigT, typename ApplicationT::MetaPropT>, ApplicationT>>>
class ServerBase {
protected:
    using PropEnumT = typename ApplicationT::PropEnumT;
    using PacketEnumT = typename ApplicationT::PacketEnumT;

private:
    ApplicationT &_app;

public:
    explicit ServerBase(ApplicationT &app) : _app(app) {};
    virtual ~ServerBase() = default;

    virtual void handle_incoming_data() = 0;

protected:
    inline ApplicationT &app() { return _app; }
};
