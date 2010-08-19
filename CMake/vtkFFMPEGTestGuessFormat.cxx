extern "C" {
#ifdef HAS_OLD_HEADER
# include <ffmpeg/avformat.h>
#else
# include <libavformat/avformat.h>
#endif
}

int main()
{
  guess_format( "avi", NULL, NULL );
  return 0;
}
