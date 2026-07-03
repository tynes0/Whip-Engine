#pragma once

namespace whip::memory
{
    struct SourceLocation
    {
        const char* File = "Unknown";
        unsigned int Line = 0;
        const char* Function = "Unknown";
    };
}
