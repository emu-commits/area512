require 'machine'
require 'io/console'
require 'area512-sprite'
require 'area512-sdfat'
require 'area512-widget'

def read_key
  input = nil
  STDIN.raw do
    input = STDIN.getch
    if input == "\e"
      a = STDIN.getch
      b = STDIN.getch
      input = input + a + b if a && b
    end
  end
  input = "" if input.nil?
  input
end
