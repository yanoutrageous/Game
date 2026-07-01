#include "App/GT_GameMode.h"

#include "App/GT_PlayerController.h"

AGT_GameMode::AGT_GameMode()
{
	PlayerControllerClass = AGT_PlayerController::StaticClass();
}
