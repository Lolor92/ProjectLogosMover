// Copyright 2026 WeirdReflection. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FSyncGASMoverModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
