# Adapted from Pytorch MNIST example: https://github.com/pytorch/examples/blob/main/mnist/main.py

from __future__ import print_function
import argparse
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torchvision import datasets, transforms
from torch.optim.lr_scheduler import StepLR

import mx

class Net(nn.Module):
    def __init__(self, mx_specs):       
        super(Net, self).__init__()

        self.mx_specs = mx_specs        

        self.conv1 = mx.Conv2d(1, 32, 3, 1, mx_specs=mx_specs)
        self.conv2 = mx.Conv2d(32, 64, 3, 1, mx_specs=mx_specs)
        self.dropout1 = nn.Dropout(0.25)	
        self.dropout2 = nn.Dropout(0.5)	
        self.fc1 = mx.Linear(9216, 128, mx_specs=mx_specs)
        self.fc2 = mx.Linear(128, 10, mx_specs=mx_specs)


    def forward(self, x):
        x = self.conv1(x)
        x = mx.relu(x, mx_specs=self.mx_specs)
        x = self.conv2(x)
        x = mx.relu(x, mx_specs=self.mx_specs)
        x = F.max_pool2d(x, 2)              
        x = self.dropout1(x)
        x = torch.flatten(x, 1)             
        x = self.fc1(x)
        x = mx.relu(x, mx_specs=self.mx_specs)
        x = self.dropout2(x)
        x = self.fc2(x)
        output = mx.simd_log(mx.softmax(x, dim=1, mx_specs=self.mx_specs), mx_specs=self.mx_specs)    

        return output


def train(args, model, device, train_loader, optimizer, epoch):
    model.train()
    for batch_idx, (data, target) in enumerate(train_loader):
        data, target = data.to(device), target.to(device)
        optimizer.zero_grad()
        output = model(data)
        loss = F.nll_loss(output, target)
        loss.backward()
        optimizer.step()
        if batch_idx % args.log_interval == 0:
            print('Train Epoch: {} [{}/{} ({:.0f}%)]\tLoss: {:.6f}'.format(
                epoch, batch_idx * len(data), len(train_loader.dataset),
                100. * batch_idx / len(train_loader), loss.item()))
            if args.dry_run:
                break


def test(model, device, test_loader):
    model.eval()
    test_loss = 0
    correct = 0
    with torch.no_grad():
        for data, target in test_loader:
            data, target = data.to(device), target.to(device)
            output = model(data)
            test_loss += F.nll_loss(output, target, reduction='sum').item()  # sum up batch loss
            pred = output.argmax(dim=1, keepdim=True)  # get the index of the max log-probability
            correct += pred.eq(target.view_as(pred)).sum().item()

    test_loss /= len(test_loader.dataset)
    
    # MODIFICATION: Calculate accuracy variable to return it
    accuracy = 100. * correct / len(test_loader.dataset)

    print('\nTest set: Average loss: {:.4f}, Accuracy: {}/{} ({:.0f}%)\n'.format(
        test_loss, correct, len(test_loader.dataset),
        accuracy))
    
    # MODIFICATION: Return accuracy for the grid search table
    return accuracy


def main():
    # Training settings
    parser = argparse.ArgumentParser(description='PyTorch MNIST Example')
    parser.add_argument('--batch-size', type=int, default=64, metavar='N',
                        help='input batch size for training (default: 64)')
    parser.add_argument('--test-batch-size', type=int, default=1000, metavar='N',
                        help='input batch size for testing (default: 1000)')
    parser.add_argument('--epochs', type=int, default=10, metavar='N',
                        help='number of epochs to train (default: 10)')
    parser.add_argument('--lr', type=float, default=1.0, metavar='LR',          
                        help='learning rate (default: 1.0)')
    parser.add_argument('--gamma', type=float, default=0.7, metavar='M',
                        help='Learning rate step gamma (default: 0.7)')
    parser.add_argument('--use-cuda', action='store_true', default=False,
                        help='disables CUDA training')
    parser.add_argument('--use-mps', action='store_true', default=False,
                        help='disables macOS GPU training')
    parser.add_argument('--dry-run', action='store_true', default=False,        
                        help='quickly check a single pass')
    parser.add_argument('--seed', type=int, default=1, metavar='S',
                        help='random seed (default: 1)')
    parser.add_argument('--log-interval', type=int, default=10, metavar='N',
                        help='how many batches to wait before logging training status')
    parser.add_argument('--save-model', action='store_true', default=False,
                        help='For Saving the current Model')
    
    args = parser.parse_args()

    use_cuda = not args.use_cuda and torch.cuda.is_available()
    use_mps = not args.use_mps and torch.backends.mps.is_available()

    torch.manual_seed(args.seed)

    if use_cuda:
        device = torch.device("cuda")
    elif use_mps:
        device = torch.device("mps")
    else:
        device = torch.device("cpu")

    train_kwargs = {'batch_size': args.batch_size}
    test_kwargs = {'batch_size': args.test_batch_size}
    if use_cuda:
        cuda_kwargs = {'num_workers': 1,
                       'pin_memory': True,
                       'shuffle': True}
        train_kwargs.update(cuda_kwargs)
        test_kwargs.update(cuda_kwargs)

    transform=transforms.Compose([
        transforms.ToTensor(),
        ])

    dataset1 = datasets.FashionMNIST('../data', train=True, download=True,
                        transform=transform)
    dataset2 = datasets.FashionMNIST('../data', train=False, download=True,
                        transform=transform)

    train_loader = torch.utils.data.DataLoader(dataset1,**train_kwargs)
    test_loader = torch.utils.data.DataLoader(dataset2, **test_kwargs)

    # =========================================================================
    #                              GRID SEARCH
    # =========================================================================

    # 1. Define lists of formats to test
    matmul_formats = ['int2', 'int8', 'fp8_e4m3', 'fp8_e5m2', 'bfloat16']
    
    # Tuples of (bfloat_setting, fp_setting, "Description")
    # Note: When bfloat=16, fp is ignored.
    elemwise_configs = [
        (16, 0, "bfloat16"), 
        (0, 16, "fp16"),
        (0, 12, "fp12"),
        (0, 10, "fp10"),
        (0, 8,  "fp8")
    ]

    results = []

    # 2. Loop through all combinations
    for mm in matmul_formats:
        for (bf_val, fp_val, desc) in elemwise_configs:
            
            print(f"\n>>> TESTING COMBINATION: MatMul={mm}, Elem={desc} <<<")

            # --- YOUR MX_SPECS CONFIGURATION LOGIC ---
            mx_specs = mx.MxSpecs() 
            
            mx_specs['block_size'] = 32 # Set to 32 to use MX formats
            mx_specs['scale_bits'] = 8  # Bits for the shared scale
            
            # Elementwise logic from your snippet
            mx_specs['bfloat'] = bf_val
            if bf_val == 0:
                mx_specs['fp'] = fp_val
            
            mx_precision = mm # Variable from loop

            # Forward pass
            mx_specs['w_elem_format'] = mx_precision        # MX format for weights 
            mx_specs['a_elem_format'] = mx_precision        # MX format for activations
            
            # mx_specs['custom_cuda'] = True # Kept commented as per your snippet
            
            assert(mx_specs != None)
            # -----------------------------------------

            # Initialize model and optimizer for this run
            model = Net(mx_specs).to(device)
            optimizer = optim.Adadelta(model.parameters(), lr=args.lr)
            scheduler = StepLR(optimizer, step_size=1, gamma=args.gamma)

            acc_5 = 0.0
            acc_10 = 0.0

            # Training Loop
            for epoch in range(1, args.epochs + 1):
                train(args, model, device, train_loader, optimizer, epoch)
                acc = test(model, device, test_loader)
                
                # Grab validation % at 5 epochs and 10 epochs
                if epoch == 5:
                    acc_5 = acc
                if epoch == 10:
                    acc_10 = acc
                    
                scheduler.step()

            # Save results
            results.append((mm, desc, acc_5, acc_10))

    # =========================================================================
    #                            FINAL RESULTS TABLE
    # =========================================================================
    print("\n\n" + "="*70)
    print(f"{'MatMul Format':<15} | {'Elem Format':<12} | {'Acc @ 5':<10} | {'Acc @ 10':<10}")
    print("="*70)
    for res in results:
        print(f"{res[0]:<15} | {res[1]:<12} | {res[2]:<10.2f} | {res[3]:<10.2f}")

    if args.save_model:
        torch.save(model.state_dict(), "fashion-mnist_cnn.pt")


if __name__ == '__main__':
    main()