"""Fit requested OpenXR eye dimensions within the current native bridge budget."""
def dimensions(width,height,scale):
 if scale==100:return width,height
 return max(64,(width*scale//100)&~3),max(64,(height*scale//100)&~3)

def select_scale(width,height,requested):
 if not isinstance(width,int) or not isinstance(height,int) or not 1<=width<=16384 or not 1<=height<=16384:raise ValueError('Invalid runtime eye dimensions')
 if not 25<=requested<=100:raise ValueError('Requested scale must be 25..100')
 for scale in range(requested,24,-1):
  w,h=dimensions(width,height,scale)
  if w<=4096 and h<=4096 and w*h*40<=256*1024*1024:return scale,w,h
 raise ValueError('Even 25% resolution exceeds the current native bridge budget')
