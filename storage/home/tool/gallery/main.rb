require 'area512-sprite'
require 'area512-widget'
require 'machine'
require 'io/console'

class WidgetGallery
  def read_key
    key = ""

    STDIN.raw do
      key = Widget.read_key
    end

    key
  end

  def run
    Display.fill_screen(Widget.theme_background)
    @sp = BandedSprite.new(12)

    begin
      until @quit
        draw
        handle(read_key)
      end
    ensure
      @sp.delete if @sp
      @list.delete if @list
      @text_view.delete if @text_view
      Display.fill_screen(Widget.theme_background)
    end
  end
end

WidgetGallery.new.run
