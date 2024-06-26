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

pub const IsoclineWriter = struct {
    pub const Error = error{};
    pub const Self = IsoclineWriter;

    pub fn writeAll(_: Self, bytes: []const u8) Error!void {
        isocline.ic_print(@ptrCast(bytes));
    }

    pub fn writeBytesNTimes(_: Self, bytes: []const u8, n: usize) Error!void {
        for (0..n) |_| {
            isocline.ic_print(@ptrCast(bytes));
        }
    }

    pub fn print(_: Self, comptime fmt: []const u8, args: anytype) Error!void {
        try std.fmt.format(IsoclineWriter{}, fmt, args);
    }
};

pub fn print(comptime fmt: []const u8, args: anytype) !void {
    const w = IsoclineWriter{};
    try w.print(fmt, args);
}

pub fn setHistory(file_name: ?[]const u8, max_entries: c_long) void {
    isocline.ic_set_history(@ptrCast(file_name), max_entries);
}

pub fn enableAutoTab(enabled: bool) void {
    isocline.ic_enable_auto_tab(enabled);
}

pub fn historyAdd(entry: []const u8) void {
    isocline.ic_history_add(@ptrCast(entry));
}

pub fn styleDef(style_name: []const u8, fmt: []const u8) void {
    isocline.ic_style_def(@ptrCast(style_name), @ptrCast(fmt));
}

test "History" {
    setHistory(null, 200);
}

test "Print" {
    try print("{s}", .{"Hello"});
}
