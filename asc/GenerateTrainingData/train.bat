train.exe -s 11 -c 1 -B 1 e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.arousal.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\arousal.model
train.exe -s 11 -c 1 -B 1 e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.valence.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\valence.model
train.exe -s 11 -c 1 -B 1 e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.power.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\power.model
train.exe -s 11 -c 1 -B 1 e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.expectancy.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\expectancy.model

predict.exe e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.arousal.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\arousal.model e:\localdata\ASC-Inclusion\avec2012\modelsFast\arousal.prediction
predict.exe e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.valence.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\valence.model e:\localdata\ASC-Inclusion\avec2012\modelsFast\valence.prediction
predict.exe e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.power.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\power.model e:\localdata\ASC-Inclusion\avec2012\modelsFast\power.prediction
predict.exe e:\localdata\ASC-Inclusion\avec2012\featuresFast\ipd21_features.expectancy.libsvm_scaled e:\localdata\ASC-Inclusion\avec2012\modelsFast\expectancy.model e:\localdata\ASC-Inclusion\avec2012\modelsFast\expectancy.prediction

pause