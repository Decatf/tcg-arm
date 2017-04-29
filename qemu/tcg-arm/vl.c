
#include "qemu/osdep.h"
// #include "qemu-version.h"
#include "qemu/cutils.h"
#include "qemu/help_option.h"

#include "qapi/string-input-visitor.h"
#include "qapi/opts-visitor.h"
#include "qom/object_interfaces.h"
#include "qapi-event.h"
#include "qapi/qmp/qerror.h"


static RunState current_run_state = RUN_STATE_PRELAUNCH;

bool runstate_check(RunState state)
{
    return current_run_state == state;
}

int runstate_is_running(void)
{
    return runstate_check(RUN_STATE_RUNNING);
}