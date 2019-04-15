import datatable as dt
from datatable import *
from datatable.models import *

dt.options.display.interactive = True

class AutoFtrl:
    def __init__(self):
        pass

    def auto(self, dt_X, dt_y, params, k):
        self.model = Ftrl()

        self.params = params
        self.keys = list(params.keys())
        self.res_names = ["loss"] + self.keys
        self.res = dt.Frame(names = self.res_names)

        self.dt_X = dt_X
        self.dt_y = dt_y

        self.iteration = 0
        self.niterations = 1
        for key in self.keys:
            self.niterations *= len(params[key])
        self.k = k
        self.kf = kfold(nrows = dt_X.nrows, nsplits = k)

        self.iterate(0)
        return self.res

    def iterate(self, index):
        if index < len(self.keys):
            key = self.keys[index]
            for value in self.params[key]:
                setattr(self.model, key, value)
                self.iterate(index + 1)
        else:
            loss = 0
            for i in range(self.k):
                dt_X = self.dt_X[self.kf[i][0], :]
                dt_y = self.dt_y[self.kf[i][0], :]
                dt_X_val = self.dt_X[self.kf[i][1], :]
                dt_y_val = self.dt_y[self.kf[i][1], :]
                res = self.model.fit(dt_X, dt_y, dt_X_val, dt_y_val)
                self.model.reset()
                loss += getattr(res, "loss")

            loss /= self.k
            values = {"loss": [loss]}
            for key in self.keys:
                values[key] = [getattr(self.model.params, key)]
            df_values = dt.Frame(values)
            self.res.rbind(df_values)
            self.iteration+=1
            print("Iteration %d/%d: %s" % (self.iteration, self.niterations, values))


file_name = "~/Downloads/kaggledays-sf-hackathon/train_2.csv"
df_train = dt.Frame(file_name)
print("")
print("Dataset:    ", file_name)
print("Shape:      ", df_train.shape)
print("")

dt_X = df_train[:, 2::]
dt_y = df_train[:, 1]

params = {
          "alpha": [0.1, 0.01, 0.001],
          # "lambda1": [0, 3, 4],
          # "nepochs": [1, 2, 3],
          "mantissa_nbits": list(range(5))
         }


af = AutoFtrl()
res = af.auto(dt_X, dt_y, params, 2)

print("")
print(res[:, :, sort(f["loss"])])


