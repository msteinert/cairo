d = Dir.glob("xml/*.xml")

ids = []

# collect symbols 
d.each do |file|
  f = File.new(file)
  buf = f.read
  ids << buf.scan(/.*id="(.*)".*/)
end
ids = ids.flatten

#resolve symbols
d.each do |file|
  f = File.new(file,"r+")
  buf = f.read
  p file
  ids.each do |id|
    if "xml/"+id+".xml" == file
      next
    end
    re = Regexp.compile('([^"\w\d])('+id+')([^"\w\d])')
    buf.gsub!(re, '\1<link linkend="\2">\2</link>\3')
    buf.gsub!(/(<\/link>)+/, '\1')
    buf.gsub!(/(<link[^>]*>)+/, '\1')
  end
  f.rewind
  f.write buf
  f.rewind
end
