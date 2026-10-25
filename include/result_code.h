#pragma once

namespace ResultCode {
    constexpr char missingDoorNumber[] = "R17";
    constexpr char invalidDoorNumber[] = "R18";
    constexpr char invalidDoorFormat[] = "R19";
    constexpr char doorOpen[] = "R20";
    constexpr char openingDoorNumber[] = "R21";
    constexpr char doorClosed[] = "R22";
    constexpr char snapshotStored[] = "R23";
    constexpr char snapshotFailed[] = "R24";
    constexpr char cameraError[] = "R25";
    constexpr char presenceNotProvided[] = "R26";
    constexpr char presenceInvalid[] = "R27";
    constexpr char operatedLocally[] = "R28";
    constexpr char imageNotReady[] = "R29";
    constexpr char doorQueueFull[] = "R30";
    constexpr char invalidCredType[] = "R31";
    constexpr char invalidPagination[] = "R32";
    constexpr char invalidAction[] = "R33";
    constexpr char invalidCredId[] = "R34";
    constexpr char credNotFound[] = "R35";
    constexpr char invalidCredName[] = "R36";
    constexpr char invalidCredValue[] = "R37";
    constexpr char invalidDateRange[] = "R38";
    constexpr char invalidDoor[] = "R39";
    constexpr char invalidUnlimited[] = "R40";
    constexpr char invalidAmount[] = "R41";
    constexpr char credChangeFailed[] = "R42";
    constexpr char credSaved[] = "R43";
    constexpr char credDeleted[] = "R44";
}
