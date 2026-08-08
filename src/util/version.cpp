//
// Avoid importing anything other than the generated header, since this file will be recompiled
// whenever the version from git describe changes (such as when the tree becomes dirty).
//
// Note it should not be compiled on every build, however: most of the time the tree will remain
// on the same commit and will remain dirty, and the python script that makes the version header
// only updates the header when it's contents have to change.
//
// Created by Campbell on 8/08/2026.
//

#include "version.gen.h"

namespace blt
{
	const char* SBLT_VERSION = SUPERBLT_VERSION_MACRO;
}
