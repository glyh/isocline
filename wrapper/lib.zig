const isocline = @cImport({
    @cInclude("isocline.h");
});

const std = @import("std");

pub fn readline(prompt_text: []const u8) ?[]const u8 {
    const ptr = @as(?[*:0]u8, isocline.ic_readline(@ptrCast(prompt_text)));
    if (ptr) |result| {
        return std.mem.span(result);
    } else {
        return null;
    }
}

pub fn printf(fmt: []const u8, ...) callconv(.C) void {
    var ap = @cVaStart();
    defer @cVaEnd(&ap);
    isocline.ic_vprintf(@ptrCast(fmt), ap);
}
