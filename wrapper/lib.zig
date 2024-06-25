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

const IsoclineWriter = struct {
    pub const Error = error{};
    pub const Self = IsoclineWriter;

    pub inline fn writeAll(_: Self, bytes: []const u8) Error!void {
        isocline.ic_print(@ptrCast(bytes));
    }

    pub inline fn writeBytesNTimes(_: Self, bytes: []const u8, n: usize) Error!void {
        for (0..n) |_| {
            isocline.ic_print(@ptrCast(bytes));
        }
    }
};

pub fn print(comptime fmt: []const u8, args: anytype) !void {
    try std.fmt.format(IsoclineWriter{}, fmt, args);
}

test "print" {
    try print("{s}", .{"Hello"});
}
