// Copyright (c) Microsoft Corporation.  All Rights Reserved.
// See LICENSE.txt in the plugin root for license information.

#pragma once

#include "IMonoRuntime.h"

/**
 * Send a command to the IDE asynchronously.
 *
 * @param launch		Launch the IDE if not already connected.
 * @param command		The command string to send to the IDE.
 * @return				True if already connected.
 */
bool MonoIdeAgent_SendCommand(bool launch, const TCHAR* command);

bool MonoIdeAgent_IsConnected();