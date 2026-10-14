const nx = @import("nexora");

export fn NexoraPointerLayout(index: u32) u64 {
    const T = nx.PointerInputSnapshotV1;
    if (index == 0) return @sizeOf(T);
    if (index == 1) return @alignOf(T);
    comptime var slot = 2;
    inline for (@typeInfo(T).@"struct".fields) |field| {
        if (index == slot) return @offsetOf(T, field.name);
        slot += 1;
    }
    if (index == slot) return @sizeOf(nx.GameplayHostV3);
    if (index == slot + 1) return @offsetOf(nx.GameplayHostV3, "capture_pointer_input");
    if (index == slot + 2) return nx.host_v3_base_size;
    if (index == slot + 3) return nx.host_v3_twod_size;
    if (index == slot + 4) return nx.host_v3_pointer_size;
    return 0xffffffffffffffff;
}

export fn NexoraPointerProbe(host: *const nx.GameplayHostV3, user: u32, output: *nx.PointerInputSnapshotV1) i32 {
    return nx.capturePointerInput(host, user, output);
}
