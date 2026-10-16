param(
    [Parameter(Mandatory=$true)][string]$TextFile,
    [Parameter(Mandatory=$true)][string]$Output,
    [string]$FontName = "Microsoft JhengHei",
    [string]$FontFile = "",
    [float]$PixelSize = 32,
    [int]$Columns = 16
)
$ErrorActionPreference = "Stop"
Add-Type -ReferencedAssemblies System, System.Core, System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
public static class NexoraFontBaker {
    [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr value);
    [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr value);
    [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr value);
    [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr value);
    [DllImport("gdi32.dll", CharSet=CharSet.Unicode, ExactSpelling=true)]
    static extern uint GetGlyphIndicesW(IntPtr dc, string text, int count, [Out] ushort[] glyphs, uint flags);
    [DllImport("gdi32.dll", CharSet=CharSet.Unicode, ExactSpelling=true)]
    static extern int AddFontResourceExW(string path, uint flags, IntPtr reserved);
    [DllImport("gdi32.dll", CharSet=CharSet.Unicode, ExactSpelling=true)]
    static extern bool RemoveFontResourceExW(string path, uint flags, IntPtr reserved);
    public static void Bake(string textFile, string output, string name, string fontFile,
                            float pixels, int columns) {
        if (pixels < 4 || pixels > 96 || Single.IsNaN(pixels) || columns < 1 || columns > 32)
            throw new ArgumentException("Invalid font size or column count.");
        if (File.Exists(output)) throw new IOException("Output must be a new file.");
        SortedSet<char> codes = new SortedSet<char>();
        codes.Add(' ');
        foreach (char c in File.ReadAllText(textFile, new UTF8Encoding(false, true))) {
            if (Char.IsSurrogate(c))
                throw new ArgumentException("This GDI atlas baker accepts BMP characters only.");
            if (!Char.IsControl(c)) codes.Add(c);
        }
        bool added = false;
        using (PrivateFontCollection privateFonts = new PrivateFontCollection()) {
            try {
                FontFamily family;
                if (fontFile.Length != 0) {
                    fontFile = Path.GetFullPath(fontFile);
                    added = AddFontResourceExW(fontFile, 0x10, IntPtr.Zero) != 0;
                    if (!added) throw new IOException("Cannot register the supplied font privately.");
                    privateFonts.AddFontFile(fontFile);
                    family = privateFonts.Families[0];
                } else {
                    family = new FontFamily(name);
                    if (!String.Equals(family.Name, name, StringComparison.OrdinalIgnoreCase))
                        throw new ArgumentException("Requested font family is unavailable.");
                }
                using (Font font = new Font(family, pixels, FontStyle.Regular, GraphicsUnit.Pixel)) {
                    IntPtr dc = CreateCompatibleDC(IntPtr.Zero), handle = font.ToHfont(), old = IntPtr.Zero;
                    try {
                        if (dc == IntPtr.Zero) throw new IOException("Cannot create a glyph inspection DC.");
                        old = SelectObject(dc, handle);
                        foreach (char c in codes) {
                            ushort[] glyph = new ushort[1];
                            if (GetGlyphIndicesW(dc, c.ToString(), 1, glyph, 1) == UInt32.MaxValue ||
                                glyph[0] == 0xffff)
                                throw new ArgumentException("Font lacks U+" + ((int)c).ToString("X4"));
                        }
                    } finally {
                        if (old != IntPtr.Zero) SelectObject(dc, old);
                        if (handle != IntPtr.Zero) DeleteObject(handle);
                        if (dc != IntPtr.Zero) DeleteDC(dc);
                    }
                    int cellWidth = (int)Math.Ceiling(pixels * 2) + 8;
                    int cellHeight;
                    using (Bitmap probe = new Bitmap(1, 1))
                    using (Graphics graphics = Graphics.FromImage(probe))
                        cellHeight = (int)Math.Ceiling(font.GetHeight(graphics)) + 8;
                    int width = cellWidth * columns, height = cellHeight * ((codes.Count + columns - 1) / columns);
                    if (width > 4096 || height > 4096 || width * 4L * height > 16L * 1024 * 1024)
                        throw new ArgumentException("Font atlas exceeds the runtime size budget.");
                    using (Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb))
                    using (Graphics graphics = Graphics.FromImage(bitmap))
                    using (StringFormat format = (StringFormat)StringFormat.GenericTypographic.Clone())
                    using (MemoryStream metrics = new MemoryStream()) {
                        bitmap.SetResolution(96, 96);
                        graphics.Clear(Color.Transparent);
                        graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
                        format.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces | StringFormatFlags.NoWrap;
                        BinaryWriter values = new BinaryWriter(metrics);
                        int index = 0;
                        foreach (char c in codes) {
                            int x = (index % columns) * cellWidth, y = (index / columns) * cellHeight;
                            string text = c.ToString();
                            float advance = graphics.MeasureString(text, font, Int32.MaxValue, format).Width;
                            graphics.DrawString(text, font, Brushes.White, x + 2, y + 2, format);
                            values.Write((uint)c); values.Write(advance);
                            values.Write(0f); values.Write(0f);
                            values.Write((float)cellWidth); values.Write((float)cellHeight);
                            values.Write((float)x / width); values.Write((float)y / height);
                            values.Write((float)(x + cellWidth) / width); values.Write((float)(y + cellHeight) / height);
                            index++;
                        }
                        using (FileStream stream = new FileStream(output, FileMode.CreateNew, FileAccess.Write))
                        using (BinaryWriter writer = new BinaryWriter(stream)) {
                            writer.Write(Encoding.ASCII.GetBytes("NXFONT1\0"));
                            writer.Write((uint)width); writer.Write((uint)height);
                            writer.Write(pixels); writer.Write((float)cellHeight); writer.Write((uint)codes.Count);
                            writer.Write(metrics.ToArray());
                            BitmapData data = bitmap.LockBits(new Rectangle(0, 0, width, height),
                                ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                            try {
                                byte[] row = new byte[width * 4];
                                for (int y = 0; y < height; y++) {
                                    Marshal.Copy(IntPtr.Add(data.Scan0, y * data.Stride), row, 0, row.Length);
                                    for (int x = 0; x < width; x++) {
                                        writer.Write((byte)255); writer.Write((byte)255);
                                        writer.Write((byte)255); writer.Write(row[x * 4 + 3]);
                                    }
                                }
                            } finally { bitmap.UnlockBits(data); }
                        }
                    }
                }
            } finally { if (added) RemoveFontResourceExW(fontFile, 0x10, IntPtr.Zero); }
        }
    }
}
'@
$destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)
if (Test-Path -LiteralPath $destination) { throw "Output must be a new file." }
if ($FontFile) { $FontFile = (Resolve-Path -LiteralPath $FontFile).Path }
$temporary = $destination + "." + [Guid]::NewGuid().ToString("N") + ".tmp"
try {
    [NexoraFontBaker]::Bake((Resolve-Path -LiteralPath $TextFile).Path,
        $temporary, $FontName, $FontFile, $PixelSize, $Columns)
    [System.IO.File]::Move($temporary, $destination)
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
}
Write-Output "Font atlas written: $Output"
