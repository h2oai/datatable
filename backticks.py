from datatable import dt
DT = dt.Frame()

try:
	D = DT["e`fg"]
except Exception as e:
	print(e)

DT["e`fg"]