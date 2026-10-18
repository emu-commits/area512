class Sandbox
  def load_manifest(directory, manifest_path, join: true)
    file = File.open(manifest_path, "r")

    begin
      text = file.read
    ensure
      file.close
    end

    return nil unless text

    lines = text.split("\n")

    i = 0
    while i < lines.size
      line = lines[i].strip
      i += 1

      next if line.empty? || line.start_with?("#")

      path = line.start_with?("/") ? line : "#{directory}/#{line}"
      prepared = area512_prepare_task_for_run(path)

      unless prepared
        raise RuntimeError, "#{path}: area512_prepare_task_for_run failed"
      end

      if join
        wait(timeout: nil)

        if task_error = error
          raise task_error
        end
      end
    end

    true
  end

  def cleanup
    terminate
    area512_release_kept_code
    free_parser
    nil
  end
end
