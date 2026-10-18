class Writer
  def draw
    while @sp.draw
      render
    end
  end

  def render
    @sp.fill(Widget.theme_background)
    Widget.header(@sp, "WRITER", DOC_NAME + dirty_mark)
    draw_page
    draw_status
  end

  def draw_page
    y = Widget.body_top
    w = Display.width
    h = Widget.body_height
    Widget.panel(@sp, 0, y, w, h)
    draw_lines(PAGE_PAD, y + PAGE_PAD, w - PAGE_PAD * 2 - SCROLLBAR_W)
    Widget.scrollbar(
      @sp,
      w - PAGE_PAD - SCROLLBAR_W,
      y + PAGE_PAD,
      h - PAGE_PAD * 2,
      @top,
      visible_rows,
      @lines.length
    )
  end

  def draw_lines(x, y, w)
    r = @top
    yy = y
    rows = visible_rows
    while r < @top + rows && r < @lines.length
      line = @lines[r]
      line = "" unless line.is_a?(String)
      @sp.text(x, yy, clip_px(line, w), Widget.theme_text)
      draw_cursor(x, yy, w) if r == @row
      yy += LINE_H
      r += 1
    end
  end

  def draw_cursor(x, y, w)
    cx = x + text_width(current_line, @col)
    max = x + w
    cx = max if cx > max
    @sp.line(cx, y - 1, cx, y + LINE_H - 2, Widget.theme_selected)
  end

  def draw_status
    pos = (@row + 1).to_s + ":" + (@col + 1).to_s
    Widget.footer(@sp, pos + " " + @msg)
  end
end
