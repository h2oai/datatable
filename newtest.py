import datatable as dt


df = dt.Frame([True, False, False, None, True, True, False])
res = df.newsort()
print(res)
