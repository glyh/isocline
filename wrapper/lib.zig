const isocline = @cImport({
    @cInclude("isocline.h");
});

pub fn readline(prompt_text: []const u8) ?[]const u8 {
    return @ptrCast(isocline.ic_readline(@ptrCast(prompt_text)));
}

pub fn printf(comptime fmt: []const u8, ...) void {
    var ap = @cVaStart();
    defer @cVaEnd(&ap);
    isocline.ic_vprintf(@ptrCast(fmt), ap);
}
