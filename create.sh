# RMat27
# (scale 27) is one of the Graph500 benchmark graphs [18], and was
# generated with parameters a = 0.57, b = c = 0.19, d = 0.05.
# num edges: 2.12 × 109
# num vertices: 1.34 × 108

#rMat27
#./utils/rMatGraph -a 0.57 -b 0.19 -c 0.19 -d 0.05 -m $((212*10000000)) $((134*1000000)) inputs/rMat27

# rMat27_smaller
 ./utils/rMatGraph -a 0.57 -b 0.19 -c 0.19 -d 0.05 -m $((212*10000000/20)) $((134*1000000/20)) inputs/rMat27_smaller
