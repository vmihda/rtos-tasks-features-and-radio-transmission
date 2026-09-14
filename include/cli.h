#pragma once

namespace Cli {

// Reads any pending Serial input, and dispatches complete lines to command
// handlers. Non-blocking — safe to call once per main loop iteration.
void poll();

} // namespace Cli
